#include "antcore_app_telemetry.h"
#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/peripherals_state.h"
#include "app/robot_state.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_inputs.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_log.h"
#include "antcore_app_network.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_robot.h"
#include <LittleFS.h>
#include "antcore_config.h"
#include "antcore_output_test.h"
#include "camera_stream.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

CommandsState commandsState;

static void finishRuntimeCommand(RuntimeCommand& command, bool ok, int status, const String& message);
static void executeRuntimeCommand(RuntimeCommand& command);

static void finishRuntimeCommand(RuntimeCommand& command, bool ok, int status, const String& message) {
  command.ok = ok;
  command.status = status;
  command.message = message;
  command.armedResult = robotState.armed;
  command.weaponArmedResult = robotState.weaponArmed;
  command.boolResult = robotState.outputTest.liveOutputEnabled;
}

static void executeRuntimeCommand(RuntimeCommand& command) {
  switch (command.type) {
    case RuntimeCommandType::Arm:
      armRobot(command.source);
      finishRuntimeCommand(command, robotState.armed, robotState.armed ? 200 : 409, robotState.armed ? "armed" : robotState.disarmReason);
      break;
    case RuntimeCommandType::Disarm:
      disarmRobot(command.source);
      finishRuntimeCommand(command, true, 200, "disarmed");
      break;
    case RuntimeCommandType::WeaponArm:
      armWeapon(command.source);
      finishRuntimeCommand(command, robotState.weaponArmed, robotState.weaponArmed ? 200 : 409,
                           robotState.weaponArmed ? "weapon armed" : robotState.weaponDisarmReason);
      break;
    case RuntimeCommandType::WeaponDisarm:
      disarmWeapon(command.source);
      finishRuntimeCommand(command, true, 200, "weapon safe");
      break;
    case RuntimeCommandType::PitSet:
      if (commandsState.filesystemOtaInProgress) {
        finishRuntimeCommand(command, false, 503, "filesystem OTA in progress");
        break;
      }
      configState.cfg.safety.pitMode = command.boolValue;
      if (configState.cfg.safety.pitMode) {
        antcore_output_test::disableLiveOutput(robotState.outputTest);
        disarmRobot("pit mode active");
      }
      sanitizeConfig();
      finishRuntimeCommand(command, saveConfig(), 200,
                           configState.cfg.safety.pitMode ? "pit mode enabled" : "pit mode disabled");
      if (!command.ok) command.status = 500;
      break;
    case RuntimeCommandType::ReleaseWebControl:
      clearWebControlLock();
      finishRuntimeCommand(command, true, 200, "web control released");
      addLog("INFO", "web driver lock released");
      break;
    case RuntimeCommandType::ApplyConfig: {
      if (commandsState.filesystemOtaInProgress) {
        finishRuntimeCommand(command, false, 503, "filesystem OTA in progress");
        break;
      }
      AppConfig nextConfig = configState.cfg;
      bool disableLiveOutput = false;
      {
        // Consume every parsed variant before saveConfig reuses the workspace.
        JsonDocument& doc = borrowLoopJsonDocument();
        DeserializationError err = deserializeJson(doc, command.text);
        if (err) {
          finishRuntimeCommand(command, false, 400, String("invalid config JSON: ") + err.c_str());
          break;
        }
        disableLiveOutput = antcore_config::applyConfigJson(nextConfig, doc.as<JsonVariantConst>());
      }
      command.text = String();
      antcore_config::sanitizeConfig(nextConfig);
      disarmRobot(command.source.length() ? command.source : "config save");
      configState.cfg = nextConfig;
      if (disableLiveOutput) antcore_output_test::disableLiveOutput(robotState.outputTest);
      sanitizeConfig();
      const bool ok = saveConfig();
      if (ok && peripheralsState.cameraReady) applyCameraSensorSettings(configState.cfg.camera);
      if (ok) scheduleStaRestart();
      finishRuntimeCommand(command, ok, ok ? 200 : 500, ok ? "config saved" : "config save failed");
      addLog(ok ? "INFO" : "ERROR", ok ? "config saved" : "config save failed");
      break;
    }
    case RuntimeCommandType::ResetConfig:
      if (commandsState.filesystemOtaInProgress) {
        finishRuntimeCommand(command, false, 503, "filesystem OTA in progress");
        break;
      }
      disarmRobot(command.source.length() ? command.source : "factory reset");
      resetDefaultConfig();
      sanitizeConfig();
      configState.prefs.remove(PREF_CONFIG_KEY);
      if (configState.fsMounted) LittleFS.remove(CONFIG_FILE_PATH);
      finishRuntimeCommand(command, saveConfig(), 200, "defaults restored");
      if (!command.ok) command.status = 500;
      scheduleStaRestart();
      addLog(command.ok ? "INFO" : "ERROR", command.ok ? "factory defaults restored" : "factory reset save failed");
      break;
    case RuntimeCommandType::SaveProfile: {
      disarmRobot("profile save");
      String reason;
      String name;
      const bool ok = saveNamedProfile(command.text, name, reason);
      command.name = name;
      finishRuntimeCommand(command, ok, ok ? 200 : 400, reason);
      addLog(ok ? "INFO" : "ERROR", ok ? "profile saved: " + name : "profile save failed: " + reason);
      break;
    }
    case RuntimeCommandType::LoadProfile: {
      if (commandsState.filesystemOtaInProgress) {
        finishRuntimeCommand(command, false, 503, "filesystem OTA in progress");
        break;
      }
      String reason;
      String name;
      const bool ok = loadNamedProfile(command.text, name, reason);
      command.name = name;
      finishRuntimeCommand(command, ok, ok ? 200 : 404, reason);
      addLog(ok ? "INFO" : "ERROR", ok ? "profile loaded: " + name : "profile load failed: " + reason);
      break;
    }
    case RuntimeCommandType::LiveOutputSet: {
      antcore_output_test::GateResult result = antcore_output_test::setLiveOutputEnabled(
          robotState.outputTest, command.boolValue, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode);
      finishRuntimeCommand(command, result.ok, result.ok ? 200 : 409,
                           result.ok ? (robotState.outputTest.liveOutputEnabled ? "live output tests enabled" : "live output tests disabled")
                                     : result.reason);
      if (result.ok) {
        addLog("WARN", robotState.outputTest.liveOutputEnabled ? "live output tests enabled" : "live output tests disabled");
      }
      break;
    }
    case RuntimeCommandType::MotorTest: {
      antcore_output_test::QueueResult result = antcore_output_test::queueMotorTest(
          robotState.outputTest, robotState.armed, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode,
          command.index, command.power, command.durationMs, millis());
      command.index = result.index;
      finishRuntimeCommand(command, result.ok,
                           result.ok ? 200 : (command.index < 1 || command.index > MOTOR_COUNT ? 400 : 409),
                           result.ok ? "motor test queued" : result.reason);
      if (result.ok) addLog("WARN", "motor test queued from web");
      break;
    }
    case RuntimeCommandType::ServoTest: {
      antcore_output_test::QueueResult result = antcore_output_test::queueServoTest(
          robotState.outputTest, robotState.armed, configState.cfg.safety.pitMode, configState.cfg.control.calibration.mappingTestMode,
          configState.cfg.servos, command.index, command.servoUs, command.durationMs, millis());
      command.index = result.index;
      finishRuntimeCommand(command, result.ok,
                           result.ok ? 200 : (command.index < 1 || command.index > SERVO_COUNT ? 400 : 409),
                           result.ok ? "servo test queued" : result.reason);
      if (result.ok) addLog("WARN", "servo test queued from web");
      break;
    }
    case RuntimeCommandType::BleForget:
      disarmRobot(command.source.length() ? command.source : "BLE forget");
      if (inputsState.bleReady) {
        inputsState.xbox.disconnect();
        NimBLEDevice::deleteAllBonds();
        BLEGamepadClient::getAutoScan()->enable();
        BLEGamepadClient::getAutoScan()->notify();
        finishRuntimeCommand(command, true, 200, "BLE bonds cleared");
        addLog("INFO", "BLE bonds cleared");
      } else {
        finishRuntimeCommand(command, false, 503,
                             lifecycleState.optionalPeripheralsSkipped ? lifecycleState.optionalPeripheralSkipReason : "BLE not ready yet");
      }
      break;
    case RuntimeCommandType::OtaBeginFirmware:
    case RuntimeCommandType::OtaBeginFilesystem:
      if (commandsState.otaInProgress) {
        finishRuntimeCommand(command, false, 409, "OTA already in progress");
        break;
      }
      disarmRobot(command.type == RuntimeCommandType::OtaBeginFirmware ? "OTA firmware" : "OTA filesystem");
      commandsState.otaInProgress = true;
      commandsState.filesystemOtaInProgress = command.type == RuntimeCommandType::OtaBeginFilesystem;
      finishRuntimeCommand(command, true, 200, "OTA started");
      addLog("INFO", String(command.type == RuntimeCommandType::OtaBeginFirmware ? "firmware" : "filesystem") +
                         " OTA started: " + command.text);
      break;
    case RuntimeCommandType::OtaFinish:
      commandsState.otaInProgress = false;
      commandsState.filesystemOtaInProgress = false;
      if (command.boolValue) {
        commandsState.restartPending = true;
        commandsState.restartAtMs = millis() + OTA_RESTART_DELAY_MS;
      }
      finishRuntimeCommand(command, command.boolValue, command.boolValue ? 200 : 500,
                           command.boolValue ? "uploaded, rebooting" : "upload failed");
      break;
  }
}

void serviceRuntimeCommandQueue(uint8_t maxCommands) {
  if (commandsState.runtimeCommandQueue == nullptr) return;
  RuntimeCommand* command = nullptr;
  for (uint8_t i = 0; i < maxCommands && xQueueReceive(commandsState.runtimeCommandQueue, &command, 0) == pdTRUE; i++) {
    if (command == nullptr) continue;
    executeRuntimeCommand(*command);
    refreshTelemetry();
    if (command->done != nullptr) xSemaphoreGive(command->done);
  }
}

bool submitRuntimeCommand(RuntimeCommand& command) {
  if (commandsState.runtimeCommandQueue == nullptr) {
    command.ok = false;
    command.status = 503;
    command.message = "runtime command queue unavailable";
    return false;
  }
  SemaphoreHandle_t done = xSemaphoreCreateBinary();
  if (done == nullptr) {
    command.ok = false;
    command.status = 503;
    command.message = "runtime command semaphore unavailable";
    return false;
  }
  command.done = done;
  RuntimeCommand* ptr = &command;
  if (xQueueSend(commandsState.runtimeCommandQueue, &ptr, pdMS_TO_TICKS(50)) != pdTRUE) {
    vSemaphoreDelete(done);
    command.done = nullptr;
    command.ok = false;
    command.status = 503;
    command.message = "runtime command queue full";
    return false;
  }
  xSemaphoreTake(done, portMAX_DELAY);
  vSemaphoreDelete(done);
  command.done = nullptr;
  return command.ok;
}

}  // namespace antcore_app
