#include "app/config_state.h"
#include "app/console_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/robot_state.h"
#include "antcore_app_console.h"
#include "antcore_app_config.h"
#include "antcore_app_inputs.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_network.h"
#include "antcore_app_robot.h"
#include "antcore_app_telemetry.h"
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "antcore_serial.h"
#include "antcore_output_test.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

ConsoleState consoleState;

static void printConfigSerial();
static void handleSerialCommand(String line);

void printStatusSerial() {
  refreshTelemetry();
  Serial.print("STATUS ");
  Serial.println(cachedStatusJson(true));
}

static void printConfigSerial() {
  JsonDocument& doc = borrowLoopJsonDocument();
  configToJson(doc, false);
  Serial.print("CONFIG ");
  serializeJson(doc, Serial);
  Serial.println();
}

static void printTasksSerial() {
  Serial.printf("TASKS freeHeap=%u maxAllocHeap=%u minFreeHeap=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()),
                static_cast<unsigned>(ESP.getMinFreeHeap()));
  // The bundled RTOS omits uxTaskGetSystemState. These supported lookups need
  // no heap allocation; names are truncated to 15 characters by this SDK.
  const char* const names[] = {"_autoScanTaskFn", "_clientEventCon", "_userCallbackQu",
                              "_callbackTask", "_sendDataFn", "nimble_host", "loopTask", "async_tcp", "wifi"};
  Serial.println("TASKS known task names; duplicate receiver lookup covers one of two tasks");
  for (const char* name : names) {
    const TaskHandle_t task = xTaskGetHandle(name);
    if (task == nullptr) continue;
    Serial.print("TASK name=");
    Serial.print(name);
    Serial.print(" stackHighWaterBytes=");
    Serial.println(static_cast<unsigned>(uxTaskGetStackHighWaterMark(task)));
  }
  Serial.println("OK tasks");
}

static void handleSerialCommand(String line) {
  const ParsedSerialCommand command = parseSerialCommand(line);
  if (command.type == SerialCommandType::None) return;
  if (command.error.length() > 0) {
    Serial.println(command.error);
    return;
  }

  switch (command.type) {
    case SerialCommandType::Ping:
      Serial.println("PONG AntCore");
      break;
    case SerialCommandType::Help:
      Serial.println(serialHelpText());
      break;
    case SerialCommandType::Status:
      printStatusSerial();
      break;
    case SerialCommandType::Tasks:
      printTasksSerial();
      break;
    case SerialCommandType::Config:
      printConfigSerial();
      break;
    case SerialCommandType::Arm:
      armRobot("serial");
      Serial.println(robotState.armed ? "OK armed" : "ERR arm blocked");
      break;
    case SerialCommandType::Disarm:
      disarmRobot("serial");
      Serial.println("OK disarmed");
      break;
    case SerialCommandType::WeaponArm:
      armWeapon("serial");
      Serial.println(robotState.weaponArmed ? "OK weapon armed" : "ERR weapon arm blocked");
      break;
    case SerialCommandType::WeaponDisarm:
      disarmWeapon("serial");
      Serial.println("OK weapon disarmed");
      break;
    case SerialCommandType::PitMode:
      configState.cfg.safety.pitMode = command.boolValue;
      if (configState.cfg.safety.pitMode) {
        antcore_output_test::disableLiveOutput(robotState.outputTest);
        disarmRobot("serial pit mode");
      }
      sanitizeConfig();
      saveConfig();
      Serial.println(configState.cfg.safety.pitMode ? "OK pit mode enabled" : "OK pit mode disabled");
      break;
    case SerialCommandType::BleScan:
      if (!inputsState.bleReady) {
        Serial.print("ERR ");
        Serial.println(lifecycleState.optionalPeripheralsSkipped ? lifecycleState.optionalPeripheralSkipReason : "BLE not ready yet");
        return;
      }
      BLEGamepadClient::getAutoScan()->enable();
      BLEGamepadClient::getAutoScan()->notify();
      Serial.println("OK BLE scan requested");
      break;
    case SerialCommandType::BleForget:
      if (!inputsState.bleReady) {
        Serial.print("ERR ");
        Serial.println(lifecycleState.optionalPeripheralsSkipped ? lifecycleState.optionalPeripheralSkipReason : "BLE not ready yet");
        return;
      }
      disarmRobot("serial BLE forget");
      inputsState.xbox.disconnect();
      NimBLEDevice::deleteAllBonds();
      BLEGamepadClient::getAutoScan()->notify();
      Serial.println("OK BLE bonds cleared");
      break;
    case SerialCommandType::WifiReconnect:
      restartStaFromConfig();
      Serial.println("OK WiFi reconnect requested");
      break;
    case SerialCommandType::ResetConfig:
      disarmRobot("serial config reset");
      resetDefaultConfig();
      sanitizeConfig();
      configState.prefs.remove(PREF_CONFIG_KEY);
      if (configState.fsMounted) LittleFS.remove(CONFIG_FILE_PATH);
      saveConfig();
      restartStaFromConfig();
      Serial.println("OK config reset");
      break;
    case SerialCommandType::LiveOutputEnable: {
      antcore_output_test::GateResult result = antcore_output_test::setLiveOutputEnabled(
          robotState.outputTest, command.boolValue, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode);
      if (!result.ok) {
        Serial.print("ERR ");
        Serial.println(result.reason);
        return;
      }
      Serial.println(robotState.outputTest.liveOutputEnabled ? "OK live output enabled" : "OK live output disabled");
      break;
    }
    case SerialCommandType::MotorTest: {
      antcore_output_test::QueueResult result = antcore_output_test::queueMotorTest(
          robotState.outputTest, robotState.armed, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode,
          command.index, command.power, command.durationMs, millis());
      if (!result.ok) {
        Serial.print("ERR ");
        Serial.println(result.reason);
        return;
      }
      Serial.println("OK motor test queued");
      break;
    }
    case SerialCommandType::ServoTest: {
      antcore_output_test::QueueResult result = antcore_output_test::queueServoTest(
          robotState.outputTest, robotState.armed, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode,
          configState.cfg.servos, command.index, command.servoUs, command.durationMs, millis());
      if (!result.ok) {
        Serial.print("ERR ");
        Serial.println(result.reason);
        return;
      }
      Serial.println("OK servo test queued");
      break;
    }
    case SerialCommandType::Unknown:
    default:
      Serial.println("ERR unknown command");
      break;
  }
}

void pollSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      handleSerialCommand(consoleState.serialLine);
      consoleState.serialLine = "";
    } else if (consoleState.serialLine.length() < 240) {
      consoleState.serialLine += c;
    }
  }
}

}  // namespace antcore_app
