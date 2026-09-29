#include "antcore_app_telemetry.h"
#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/log_state.h"
#include "app/network_state.h"
#include "app/peripherals_state.h"
#include "app/web_state.h"
#include "app/ws_state.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_console.h"
#include "antcore_app_indicator.h"
#include "antcore_app_inputs.h"
#include "antcore_app_log.h"
#include "antcore_app_network.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_robot.h"
#include "antcore_app_web.h"
#include "antcore_app_ws.h"
#include <LittleFS.h>
#include <esp_system.h>
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

LifecycleState lifecycleState;

RTC_DATA_ATTR uint32_t rtcBootGuardMagic = 0;
RTC_DATA_ATTR uint8_t rtcBootCrashCount = 0;

static const char* resetReasonText(esp_reset_reason_t reason);
static bool resetReasonSuggestsEarlyBootFailure(const String& reason);
static void startBootGuard();
static void serviceBootGuard(uint32_t now);
static void serviceOptionalPeripherals(uint32_t now);

static const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_EXT:
      return "external reset";
    case ESP_RST_SW:
      return "software reset";
    case ESP_RST_PANIC:
      return "exception/panic";
    case ESP_RST_INT_WDT:
      return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
      return "task watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep sleep wake";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "SDIO reset";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown";
  }
}

static bool resetReasonSuggestsEarlyBootFailure(const String& reason) {
  return reason == "brownout" || reason == "exception/panic" || reason == "interrupt watchdog" ||
         reason == "task watchdog" || reason == "watchdog";
}

static void startBootGuard() {
  if (rtcBootGuardMagic != BOOT_GUARD_MAGIC) {
    rtcBootGuardMagic = BOOT_GUARD_MAGIC;
    rtcBootCrashCount = 0;
  }
  lifecycleState.optionalPeripheralsSkipped = rtcBootCrashCount > 0 && resetReasonSuggestsEarlyBootFailure(lifecycleState.bootResetReason);
  if (lifecycleState.optionalPeripheralsSkipped) {
    lifecycleState.optionalPeripheralSkipReason =
        "safe boot after " + lifecycleState.bootResetReason + " during optional peripheral startup";
  }
  if (rtcBootCrashCount < 250) {
    rtcBootCrashCount++;
  }
}

static void serviceBootGuard(uint32_t now) {
  if (!lifecycleState.bootMarkedStable && now >= BOOT_STABLE_MS) {
    lifecycleState.bootMarkedStable = true;
    rtcBootCrashCount = 0;
    addLog("INFO", "boot marked stable");
  }
}

static void serviceOptionalPeripherals(uint32_t now) {
  if (lifecycleState.optionalPeripheralStartMs == 0) return;
  if (lifecycleState.optionalPeripheralsSkipped) return;

  if (!inputsState.bleReady && now >= lifecycleState.optionalPeripheralStartMs) {
    initBle();
  }

  if (ANTCORE_HAS_CAMERA && !peripheralsState.cameraInitAttempted && inputsState.bleReady && now >= lifecycleState.optionalPeripheralStartMs + CAMERA_INIT_EXTRA_DELAY_MS) {
    peripheralsState.cameraInitAttempted = true;
    initCamera();
  }
}

void setupApplication() {
  Serial.begin(SERIAL_BAUD);
  delay(700);
  logState.logMutex = xSemaphoreCreateMutex();
  commandsState.runtimeCommandQueue = xQueueCreate(RUNTIME_COMMAND_QUEUE_SIZE, sizeof(RuntimeCommand*));
  lifecycleState.serialConnectedAtBoot = static_cast<bool>(Serial);
  Serial.println();
  Serial.println("Ant Core boot");
  Serial.println(BOARD_NAME);
  lifecycleState.bootResetReason = resetReasonText(esp_reset_reason());
  startBootGuard();

  resetDefaultConfig();
  initStatusLed();
  initPins();

  configState.prefs.begin(PREF_NAMESPACE, false);
  configState.fsMounted = LittleFS.begin(true);
  addLog(configState.fsMounted ? "INFO" : "WARN", configState.fsMounted ? "LittleFS mounted" : "LittleFS mount failed");
  loadConfig();
  applyHeadlessWifiDefaults();
  disarmServos();
  addLog("INFO", "reset reason: " + lifecycleState.bootResetReason);
  if (configState.fsMounted && ensureProfileDir()) {
    addLog("INFO", "profile storage ready");
  }
  initImu();
  sampleBattery();
  initWiFiAp();
  if (configState.cfg.wifi.staEnabled) {
    startStaConnect();
  } else {
    addLog("INFO", "STA disabled; AP-only mode");
  }
  initMdns();

  initWebSocket();
  webState.server.addHandler(&wsState.ws);
  registerApiRoutes();
  initTelemetry();
  webState.server.begin();
  webState.webServerReady = true;
  addLog("INFO", "web server ready");
  if (lifecycleState.optionalPeripheralsSkipped) {
    addLog("WARN", lifecycleState.optionalPeripheralSkipReason);
  } else {
    lifecycleState.optionalPeripheralStartMs = millis() + OPTIONAL_PERIPHERAL_DELAY_MS;
    addLog("INFO", ANTCORE_HAS_CAMERA ? "BLE and camera startup deferred until network is available"
                                     : "BLE startup deferred; camera disabled on this board");
  }
  serviceLogQueue(16);
  serviceBlackboxQueue(4);
  printStatusSerial();
}

void loopApplication() {
  serviceRuntimeCommandQueue();
  pollSerial();
  if (networkState.captiveDnsReady) {
    networkState.captiveDns.processNextRequest();
  }
  uint32_t now = millis();
  serviceBootGuard(now);
  servicePendingStaRestart();
  serviceStaReconnect();
  serviceOptionalPeripherals(now);

  if (now - lifecycleState.lastSensorMs >= SENSOR_SAMPLE_MS) {
    lifecycleState.lastSensorMs = now;
    sampleBattery();
    sampleImu();
  }

  updateOutputs();
  serviceLogQueue();
  serviceBlackboxQueue();
  serviceStatusLed(now);

  if (now - lifecycleState.lastStatusBroadcastMs >= STATUS_BROADCAST_MS) {
    lifecycleState.lastStatusBroadcastMs = now;
    refreshTelemetry();
    broadcastStatus();
    wsState.ws.cleanupClients();
  }

  if (commandsState.restartPending && static_cast<int32_t>(now - commandsState.restartAtMs) >= 0) {
    stopAllMotors();
    disarmServos();
    ESP.restart();
  }

  delay(2);
}

}  // namespace antcore_app
