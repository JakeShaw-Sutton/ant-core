#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/indicator_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/network_state.h"
#include "app/peripherals_state.h"
#include "app/robot_state.h"
#include "app/web_state.h"
#include "app/ws_state.h"
#include "antcore_app_telemetry.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_indicator.h"
#include "antcore_app_inputs.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_log.h"
#include "antcore_app_network.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_robot.h"
#include "antcore_app_web.h"
#include "antcore_app_ws.h"
#include <WiFi.h>
#include "antcore_controls.h"
#include "antcore_output_test.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

namespace {
SemaphoreHandle_t snapshotMutex = nullptr;
String publicStatus = "{}";
String privateStatus = "{}";
String publicConfig = "{}";
String configValidation = "{}";

DynamicJsonDocument& telemetryScratch() {
  // Setup first borrows this before BLE startup. The loop task reuses this
  // contiguous allocation for configuration and telemetry, with no async users.
  static DynamicJsonDocument document(STATUS_JSON_CAPACITY);
  return document;
}

void reportTelemetryError(const JsonDocument& doc, const char* reason) {
  static bool reported = false;
  static uint32_t lastReportMs = 0;
  const uint32_t now = millis();
  if (reported && now - lastReportMs < 5000) return;
  reported = true;
  lastReportMs = now;
  char message[LOG_LINE_MAX_LEN];
  snprintf(message, sizeof(message), "telemetry %s: capacity=%u used=%u freeHeap=%u maxAllocHeap=%u",
           reason, static_cast<unsigned>(doc.capacity()), static_cast<unsigned>(doc.memoryUsage()),
           static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
  addLog("ERROR", message);
}

void publishSnapshot(String& cache, JsonDocument& doc) {
  const char* reason = nullptr;
  const char* error = nullptr;
  if (doc.capacity() == 0) {
    reason = "document allocation failed";
    error = "{\"ok\":false,\"message\":\"telemetry allocation failed\"}";
  } else if (doc.overflowed()) {
    reason = "overflow";
    error = "{\"ok\":false,\"message\":\"telemetry overflow\"}";
  }
  const size_t length = measureJson(doc);
  xSemaphoreTake(snapshotMutex, portMAX_DELAY);
  // Reuse each published buffer. Replacing four large allocations every
  // 200 ms fragmented C3 RAM between long-lived HTTP and BLE allocations.
  // Readers hold this same mutex, so they cannot observe a partial document.
  if (!error && !cache.reserve(length)) {
    reason = "string allocation failed";
    error = "{\"ok\":false,\"message\":\"telemetry allocation failed\"}";
  }
  if (!error) {
    cache.remove(0);
    if (serializeJson(doc, cache) != length || cache.length() != length) {
      reason = "serialization failed";
      error = "{\"ok\":false,\"message\":\"telemetry serialization failed\"}";
    }
  }
  if (error) cache = error;
  xSemaphoreGive(snapshotMutex);
  if (reason) reportTelemetryError(doc, reason);
}
}



JsonDocument& borrowLoopJsonDocument() {
  JsonDocument& document = telemetryScratch();
  document.clear();
  return document;
}

static void controlStateToJson(JsonObject obj, const ControlState& state, uint32_t now);

static void controlStateToJson(JsonObject obj, const ControlState& state, uint32_t now) {
  obj["ageMs"] = state.lastMs == 0 ? 0 : now - state.lastMs;
  JsonObject axes = obj.createNestedObject("axes");
  axes["leftX"] = state.leftX;
  axes["leftY"] = state.leftY;
  axes["rightX"] = state.rightX;
  axes["rightY"] = state.rightY;
  axes["leftTrigger"] = state.leftTrigger;
  axes["rightTrigger"] = state.rightTrigger;
  JsonObject buttons = obj.createNestedObject("buttons");
  buttons["a"] = state.a;
  buttons["b"] = state.b;
  buttons["x"] = state.x;
  buttons["y"] = state.y;
  buttons["leftBumper"] = state.leftBumper;
  buttons["rightBumper"] = state.rightBumper;
  buttons["leftStickButton"] = state.leftStickButton;
  buttons["rightStickButton"] = state.rightStickButton;
  buttons["dpadUp"] = state.dpadUp;
  buttons["dpadDown"] = state.dpadDown;
  buttons["dpadLeft"] = state.dpadLeft;
  buttons["dpadRight"] = state.dpadRight;
  buttons["menu"] = state.menu;
  buttons["view"] = state.view;
  buttons["share"] = state.share;
  buttons["xbox"] = state.xbox;
}

static void buildStatusJson(JsonDocument& doc, bool includeSensitive) {
  doc.clear();
  const auto web = webControlSnapshot();
  const uint32_t now = millis();
  const auto& webSnapshot = web.frame;
  const char* webDriverSnapshot = web.clientId;
  const bool webDriverFresh = web.locked(now);
  const bool webInputFresh = web.fresh(now);
  doc["type"] = "status";
  doc["schema"] = CONFIG_SCHEMA;
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  doc["board"] = BOARD_NAME;
  doc["filesystemVersion"] = WEB_ASSET_VERSION;
  doc["robotName"] = configState.cfg.robotName;
  doc["activeProfile"] = configState.cfg.activeProfile;
  doc["uptimeMs"] = now;
  doc["armed"] = robotState.armed;
  doc["disarmReason"] = robotState.disarmReason;
  doc["otaInProgress"] = commandsState.otaInProgress;
  doc["restartPending"] = commandsState.restartPending;
  doc["liveOutputEnabled"] = robotState.outputTest.liveOutputEnabled;
  doc["driveInverted"] = robotState.driveInverted;

  JsonObject garage = doc.createNestedObject("garage");
  garage["botType"] = configState.cfg.garage.botType;
  garage["weaponType"] = configState.cfg.garage.weaponType;
  garage["accent"] = configState.cfg.garage.accent;
  garage["avatar"] = configState.cfg.garage.avatar;
  garage["notes"] = configState.cfg.garage.notes;

  JsonObject events = doc.createNestedObject("events");
  events["controlDisconnects"] = robotState.eventCounters.controlDisconnects;
  events["failsafes"] = robotState.eventCounters.failsafes;
  events["weaponArms"] = robotState.eventCounters.weaponArms;
  events["disarms"] = robotState.eventCounters.disarms;

  JsonObject system = doc.createNestedObject("system");
  system["resetReason"] = lifecycleState.bootResetReason;
  system["freeHeap"] = ESP.getFreeHeap();
  system["maxAllocHeap"] = ESP.getMaxAllocHeap();
  system["minFreeHeap"] = ESP.getMinFreeHeap();
  system["freePsram"] = ESP.getFreePsram();
  system["bootCrashCount"] = rtcBootCrashCount;
  system["bootStable"] = lifecycleState.bootMarkedStable;
  system["optionalPeripheralsSkipped"] = lifecycleState.optionalPeripheralsSkipped;
  system["optionalPeripheralSkipReason"] = lifecycleState.optionalPeripheralSkipReason;
  system["statusIndicator"] = indicatorState.statusIndicator;
  system["statusLedPrimary"] = indicatorState.statusLedAvailable;
  system["statusLedAux"] = indicatorState.auxStatusLedAvailable;

  JsonObject security = doc.createNestedObject("security");
  security["authEnabled"] = configState.cfg.security.authEnabled;
  security["sessionActive"] = antcore_auth::sessionActive(webState.authState);
  security["defaultPin"] = includeSensitive ? isDefaultAdminPin() : false;
  security["authFailures"] = antcore_auth::failures(webState.authState);
  security["authLockoutMs"] = antcore_auth::lockoutRemainingMs(webState.authState, now);
  security["openMode"] = !configState.cfg.security.authEnabled;
  security["webClients"] = wsState.wsClientCount;
  security["statusLed"] = indicatorState.statusLedAvailable;

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["mode"] = configState.cfg.wifi.staEnabled ? "ap_sta" : "ap";
  wifi["staEnabled"] = configState.cfg.wifi.staEnabled;
  wifi["mdns"] = networkState.mdnsReady ? String(MDNS_HOSTNAME) + ".local" : "";
  // Legacy keys stay as AP values so older tools keep working.
  wifi["ssid"] = networkState.apSsid;
  wifi["ip"] = WiFi.softAPIP().toString();
  wifi["clients"] = WiFi.softAPgetStationNum();
  JsonObject ap = wifi.createNestedObject("ap");
  ap["ssid"] = networkState.apSsid;
  ap["ip"] = WiFi.softAPIP().toString();
  ap["clients"] = WiFi.softAPgetStationNum();
  ap["captiveDns"] = networkState.captiveDnsReady;
  JsonObject sta = wifi.createNestedObject("sta");
  sta["enabled"] = configState.cfg.wifi.staEnabled;
  sta["ssid"] = includeSensitive ? configState.cfg.wifi.staSsid : "";
  sta["connected"] = staConnected();
  sta["ip"] = staConnected() ? WiFi.localIP().toString() : "";
  sta["rssi"] = staConnected() ? WiFi.RSSI() : 0;
  sta["everConnected"] = networkState.staEverConnected;
  sta["headlessDefaultApplied"] = configState.headlessStaDefaultsApplied;
  sta["serialConnectedAtBoot"] = lifecycleState.serialConnectedAtBoot;

  JsonObject controller = doc.createNestedObject("controller");
  controller["bleReady"] = inputsState.bleReady;
  controller["xboxConnected"] = inputsState.bleReady && inputsState.xbox.isConnected();
  controller["xboxConnecting"] = inputsState.bleReady && inputsState.xbox.isConnecting();
  controller["bleScanEnabled"] = inputsState.bleReady ? BLEGamepadClient::getAutoScan()->isEnabled() : false;
  controller["bleScanning"] = inputsState.bleReady ? BLEGamepadClient::getAutoScan()->isScanning() : false;
  controller["armButton"] = configState.cfg.control.armButton;
  controller["xboxArmEnabled"] = configState.cfg.control.xboxArmEnabled;
  const auto availableSource = antcore::selectControlSource(web, xboxControlFresh(now), now);
  const auto selectedSource = robotState.armed ? robotState.armedControlOwner.source : availableSource;
  controller["source"] = selectedSource == antcore::ControlSource::Web ? "web"
      : selectedSource == antcore::ControlSource::Xbox ? "xbox"
      : selectedSource == antcore::ControlSource::Test ? "test" : "none";
  controller["webDriverLocked"] = webDriverFresh;
  controller["webDriverId"] = webDriverSnapshot;
  controller["webDriverAgeMs"] = web.claimed ? now - web.lastClaimMs : 0;
  controller["webClients"] = wsState.wsClientCount;
  controller["driveInverted"] = robotState.driveInverted;
  controller["autoDriveInverted"] = robotState.autoDriveInverted;
  controller["effectiveDriveInverted"] = robotState.driveInverted ^ robotState.autoDriveInverted;
  controller["gyroAssistActive"] = configState.cfg.drive.gyroAssist && peripheralsState.imuTelemetry.present;
  controller["gyroHeadingZ"] = robotState.gyroHeadingZ;
  controller["gyroTargetZ"] = robotState.gyroTargetZ;
  JsonObject selfRight = controller.createNestedObject("selfRight");
  selfRight["enabled"] = configState.cfg.control.selfRight.enabled;
  selfRight["target"] = configState.cfg.control.selfRight.target;
  selfRight["active"] = antcore::timedActionActive(now, robotState.selfRightUntilMs, robotState.selfRightActive);
  selfRight["cooldownRemainingMs"] =
      antcore::cooldownRemainingMs(now, robotState.lastSelfRightMs, configState.cfg.control.selfRight.cooldownMs);
  JsonObject calibration = controller.createNestedObject("calibration");
  calibration["enabled"] = configState.cfg.control.calibration.enabled;
  calibration["mappingTestMode"] = configState.cfg.control.calibration.mappingTestMode;
  JsonArray calibrationAxes = calibration.createNestedArray("axes");
  for (uint8_t i = 0; i < AXIS_COUNT; i++) {
    JsonObject axis = calibrationAxes.createNestedObject();
    axis["name"] = AXIS_NAMES[i];
    axis["raw"] = readAnalogInputByName(inputsState.xboxRawState, AXIS_NAMES[i]);
    axis["calibrated"] = readAnalogInputByName(inputsState.xboxState, AXIS_NAMES[i]);
    axis["min"] = configState.cfg.control.calibration.axes[i].minValue;
    axis["center"] = configState.cfg.control.calibration.axes[i].centerValue;
    axis["max"] = configState.cfg.control.calibration.axes[i].maxValue;
    axis["deadband"] = configState.cfg.control.calibration.axes[i].deadband;
    axis["invert"] = configState.cfg.control.calibration.axes[i].invert;
  }
  JsonArray actionSlots = controller.createNestedArray("actionSlots");
  for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
    JsonObject slot = actionSlots.createNestedObject();
    slot["index"] = i + 1;
    slot["enabled"] = configState.cfg.control.actions[i].enabled;
    slot["button"] = configState.cfg.control.actions[i].button;
    slot["action"] = configState.cfg.control.actions[i].action;
    slot["pressed"] = configState.cfg.control.actions[i].enabled && readButtonByName(robotState.activeState, configState.cfg.control.actions[i].button);
  }
  JsonObject xboxRaw = controller.createNestedObject("xboxRaw");
  controlStateToJson(xboxRaw, inputsState.xboxRawState, now);
  JsonObject xboxCalibrated = controller.createNestedObject("xboxCalibrated");
  controlStateToJson(xboxCalibrated, inputsState.xboxState, now);
  JsonObject webRaw = controller.createNestedObject("webRaw");
  controlStateToJson(webRaw, webSnapshot, now);
  JsonObject activeRaw = controller.createNestedObject("active");
  controlStateToJson(activeRaw, robotState.activeState, now);

  JsonObject batteryObj = doc.createNestedObject("battery");
  batteryObj["supported"] = static_cast<bool>(ANTCORE_HAS_BATTERY_SENSE);
  batteryObj["dividerMultiplier"] = BATTERY_DIVIDER_MULTIPLIER;
  batteryObj["enabled"] = ANTCORE_HAS_BATTERY_SENSE && configState.cfg.battery.enabled;
  batteryObj["adcVolts"] = peripheralsState.battery.adcVolts;
  batteryObj["packVolts"] = peripheralsState.battery.packVolts;
  batteryObj["cellVolts"] = peripheralsState.battery.cellVolts;
  batteryObj["warn"] = peripheralsState.battery.warn;
  batteryObj["critical"] = peripheralsState.battery.critical;
  batteryObj["benchMode"] = configState.cfg.battery.benchMode;
  batteryObj["derating"] = peripheralsState.battery.derating;

  JsonObject imuObj = doc.createNestedObject("imu");
  imuObj["present"] = peripheralsState.imuTelemetry.present;
  imuObj["address"] = peripheralsState.imuTelemetry.address;
  imuObj["ax"] = peripheralsState.imuTelemetry.ax;
  imuObj["ay"] = peripheralsState.imuTelemetry.ay;
  imuObj["az"] = peripheralsState.imuTelemetry.az;
  imuObj["gx"] = peripheralsState.imuTelemetry.gx;
  imuObj["gy"] = peripheralsState.imuTelemetry.gy;
  imuObj["gz"] = peripheralsState.imuTelemetry.gz;

  JsonObject camera = doc.createNestedObject("camera");
  camera["supported"] = static_cast<bool>(ANTCORE_HAS_CAMERA);
  camera["ready"] = peripheralsState.cameraReady;
  camera["frameSize"] = configState.cfg.camera.frameSize;
  camera["jpegQuality"] = configState.cfg.camera.jpegQuality;
  camera["brightness"] = configState.cfg.camera.brightness;
  camera["contrast"] = configState.cfg.camera.contrast;
  camera["saturation"] = configState.cfg.camera.saturation;
  camera["hmirror"] = configState.cfg.camera.hmirror;
  camera["vflip"] = configState.cfg.camera.vflip;
  camera["streamUrl"] = peripheralsState.cameraReady ? "/stream" : "";
  camera["snapshotUrl"] = peripheralsState.cameraReady ? "/snapshot.jpg" : "";
  camera["apStreamUrl"] = cameraUrlForIp(WiFi.softAPIP(), "/stream");
  camera["apSnapshotUrl"] = cameraUrlForIp(WiFi.softAPIP(), "/snapshot.jpg");
  camera["staStreamUrl"] = staConnected() ? cameraUrlForIp(WiFi.localIP(), "/stream") : "";
  camera["staSnapshotUrl"] = staConnected() ? cameraUrlForIp(WiFi.localIP(), "/snapshot.jpg") : "";
  camera["lastError"] = peripheralsState.lastCameraError;
  camera["frames"] = peripheralsState.cameraStats.frames;
  camera["snapshots"] = peripheralsState.cameraStats.snapshots;
  camera["activeClients"] = peripheralsState.cameraStats.activeClients;
  camera["lastFrameAgeMs"] = peripheralsState.cameraStats.lastFrameAtMs == 0 ? 0 : now - peripheralsState.cameraStats.lastFrameAtMs;
  camera["lastFrameBytes"] = peripheralsState.cameraStats.lastFrameBytes;
  camera["fps"] = peripheralsState.cameraStats.fps;
  camera["avgCaptureMs"] = peripheralsState.cameraStats.avgCaptureMs;
  camera["avgSendMs"] = peripheralsState.cameraStats.avgSendMs;

  JsonObject weapon = doc.createNestedObject("weapon");
  weapon["enabled"] = configState.cfg.weapon.enabled;
  weapon["armed"] = robotState.weaponArmed;
  weapon["profile"] = configState.cfg.weapon.profile;
  weapon["requireDedicatedArm"] = configState.cfg.weapon.requireDedicatedArm;
  weapon["disarmReason"] = robotState.weaponDisarmReason;
  weapon["target"] = configState.cfg.weapon.motor < MOTOR_COUNT ? robotState.motorTarget[configState.cfg.weapon.motor] : 0.0f;
  weapon["profileOutput"] = robotState.weaponProfileOut;
  weapon["rampUpPerSecond"] = configState.cfg.weapon.rampUpPerSecond;
  weapon["rampDownPerSecond"] = configState.cfg.weapon.rampDownPerSecond;

  JsonObject safety = doc.createNestedObject("safety");
  JsonObject validation = safety.createNestedObject("config");
  const bool configValid = buildConfigValidation(validation);
  String armReason;
  safety["canArm"] = canArm(armReason, "status");
  safety["armBlockReason"] = armReason;
  safety["configValid"] = configValid;
  safety["pitMode"] = configState.cfg.safety.pitMode;
  safety["mappingTestMode"] = configState.cfg.control.calibration.mappingTestMode;
  safety["requireControlSource"] = configState.cfg.safety.requireControlSource;
  safety["benchMode"] = configState.cfg.battery.benchMode;
  safety["batteryCritical"] = peripheralsState.battery.critical;
  safety["batteryWarn"] = peripheralsState.battery.warn;
  safety["otaInProgress"] = commandsState.otaInProgress;
  safety["activeControlAvailable"] = availableSource != antcore::ControlSource::None;
  safety["defaultAdminPin"] = isDefaultAdminPin();

  JsonArray motorArray = doc.createNestedArray("motors");
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    JsonObject motor = motorArray.createNestedObject();
    motor["index"] = i + 1;
    motor["target"] = robotState.motorTarget[i];
    motor["output"] = robotState.motorOut[i];
    motor["pwmA"] = motorDutyForSide(robotState.motorOut[i], true);
    motor["pwmB"] = motorDutyForSide(robotState.motorOut[i], false);
    motor["pwmMax"] = MOTOR_PWM_MAX;
    const bool motorTest = antcore_output_test::motorTestActive(robotState.outputTest, i, now);
    motor["override"] = !robotState.armed ? "disarmed" : commandsState.otaInProgress ? "ota" : motorTest ? "test" : "live";
  }

  JsonArray servoArray = doc.createNestedArray("servos");
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    JsonObject servo = servoArray.createNestedObject();
    servo["index"] = i + 1;
    servo["attached"] = robotState.servoAttached[i];
    servo["us"] = robotState.servoOutUs[i];
    servo["failsafeUs"] = configState.cfg.servos[i].failsafeUs;
    servo["neutralUs"] = configState.cfg.servos[i].neutralUs;
    servo["minUs"] = configState.cfg.servos[i].minUs;
    servo["maxUs"] = configState.cfg.servos[i].maxUs;
    const bool servoTest = antcore_output_test::servoTestActive(robotState.outputTest, i, now);
    const bool selfRightServo = antcore::timedActionActive(now, robotState.selfRightUntilMs, robotState.selfRightActive) &&
                                ((!strcmp(configState.cfg.control.selfRight.target, "servo1") && i == 0) ||
                                 (!strcmp(configState.cfg.control.selfRight.target, "servo2") && i == 1));
    servo["override"] = !robotState.armed ? (configState.cfg.servos[i].detachOnDisarm ? "detached" : "failsafe")
                               : commandsState.otaInProgress ? "ota"
                                               : servoTest ? "test" : selfRightServo ? "self-right" : "live";
    JsonArray toggles = servo.createNestedArray("buttonToggles");
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      if (!configState.cfg.servos[i].buttons[b].enabled || !configState.cfg.servos[i].buttons[b].toggle) continue;
      JsonObject toggle = toggles.createNestedObject();
      toggle["index"] = b + 1;
      toggle["button"] = configState.cfg.servos[i].buttons[b].button;
      toggle["latched"] = robotState.servoButtonToggleLatched[i][b];
    }
  }

  JsonArray logs = doc.createNestedArray("logs");
  if (includeSensitive) {
    addLogsToJson(logs, STATUS_LOG_MAX_LINES);
  }
}

void initTelemetry() {
  snapshotMutex = xSemaphoreCreateMutex();
  // Reserve normal C3 snapshot headroom before Wi-Fi/BLE allocate their tasks.
  // publishSnapshot still checks capacity if a configuration needs more room.
  if (ANTCORE_SHARED_MOTOR_PWM) {
    bool reserved = publicStatus.reserve(7168);
    reserved = privateStatus.reserve(8192) && reserved;
    reserved = publicConfig.reserve(4096) && reserved;
    reserved = configValidation.reserve(1024) && reserved;
    if (!reserved) addLog("ERROR", "telemetry cache headroom allocation failed");
  }
  refreshTelemetry();
}

void refreshTelemetry() {
  if (!snapshotMutex) return;
  DynamicJsonDocument& status = telemetryScratch();
  buildStatusJson(status, false);
  publishSnapshot(publicStatus, status);
  buildStatusJson(status, true);
  publishSnapshot(privateStatus, status);
  status.clear();
  JsonDocument& config = status;
  configToJson(config, false);
  publishSnapshot(publicConfig, config);
  status.clear();
  JsonDocument& validation = status;
  buildConfigValidation(validation.to<JsonObject>());
  publishSnapshot(configValidation, validation);
}

String cachedStatusJson(bool includeSensitive) {
  if (!snapshotMutex) return "{}";
  xSemaphoreTake(snapshotMutex, portMAX_DELAY);
  String result = includeSensitive ? privateStatus : publicStatus;
  xSemaphoreGive(snapshotMutex);
  return result;
}

String cachedConfigJson() {
  if (!snapshotMutex) return "{}";
  xSemaphoreTake(snapshotMutex, portMAX_DELAY);
  String result = publicConfig;
  xSemaphoreGive(snapshotMutex);
  return result;
}

String cachedValidationJson() {
  if (!snapshotMutex) return "{}";
  xSemaphoreTake(snapshotMutex, portMAX_DELAY);
  String result = configValidation;
  xSemaphoreGive(snapshotMutex);
  return result;
}

}  // namespace antcore_app
