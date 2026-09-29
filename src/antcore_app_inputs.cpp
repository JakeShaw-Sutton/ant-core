#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/robot_state.h"
#include "antcore_app_inputs.h"
#include "antcore_app_config.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_log.h"
#include "antcore_app_robot.h"
#include "antcore_ble_input.h"
#include "antcore_controls.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

InputsState inputsState;

static void publishXboxControlsFromBle(const XboxControlsState& value);

antcore::WebControlSnapshot webControlSnapshot() {
  portENTER_CRITICAL(&inputsState.webControlMux);
  const auto snapshot = inputsState.webControlOwner.snapshot();
  portEXIT_CRITICAL(&inputsState.webControlMux);
  return snapshot;
}



void getWebControlSnapshot(ControlState& state, char* driverId, size_t driverIdLen, uint32_t& driverLastMs) {
  const auto snapshot = webControlSnapshot();
  state = snapshot.frame;
  if (driverId != nullptr && driverIdLen > 0) strlcpy(driverId, snapshot.clientId, driverIdLen);
  driverLastMs = snapshot.lastClaimMs;
}

void clearWebControlLock() {
  portENTER_CRITICAL(&inputsState.webControlMux);
  inputsState.webControlOwner.clear();
  portEXIT_CRITICAL(&inputsState.webControlMux);
}

bool claimWebDriverId(uint32_t connectionId, const char* id, uint32_t now) {
  portENTER_CRITICAL(&inputsState.webControlMux);
  const bool claimed = inputsState.webControlOwner.claim(connectionId, id, now);
  portEXIT_CRITICAL(&inputsState.webControlMux);
  return claimed;
}

bool releaseWebDriverId(uint32_t connectionId) {
  portENTER_CRITICAL(&inputsState.webControlMux);
  const bool released = inputsState.webControlOwner.release(connectionId);
  portEXIT_CRITICAL(&inputsState.webControlMux);
  return released;
}

bool publishWebControlFrame(uint32_t connectionId, const char* id, const ControlState& state, uint32_t now) {
  portENTER_CRITICAL(&inputsState.webControlMux);
  const bool published = inputsState.webControlOwner.publish(connectionId, id, state, now);
  portEXIT_CRITICAL(&inputsState.webControlMux);
  return published;
}

bool xboxControlFresh(uint32_t now) {
  return inputsState.bleReady && inputsState.xbox.isConnected() && inputsState.xboxState.lastMs != 0 && now - inputsState.xboxState.lastMs <= XBOX_CONTROL_STALE_MS;
}

bool hasFreshControlSource() {
  const auto web = webControlSnapshot();
  const uint32_t now = millis();
  return antcore::selectControlSource(web, xboxControlFresh(now), now) != antcore::ControlSource::None;
}

static void publishXboxControlsFromBle(const XboxControlsState& value) {
  portENTER_CRITICAL(&inputsState.xboxInputMux);
  inputsState.pendingXboxControls = value;
  inputsState.pendingXboxControlsMs = millis();
  inputsState.pendingXboxControlsAvailable = true;
  portEXIT_CRITICAL(&inputsState.xboxInputMux);
}

void updateXboxStateFromController(uint32_t now) {
  if (!inputsState.bleReady) return;
  if (inputsState.pendingXboxDisconnected) {
    inputsState.pendingXboxDisconnected = false;
    robotState.eventCounters.controlDisconnects++;
    disarmRobot("Xbox disconnected");
    inputsState.xboxState = ControlState();
    inputsState.xboxRawState = ControlState();
    inputsState.lastXboxMenu = false;
    addLog("WARN", "Xbox controller disconnected");
  }
  if (!inputsState.xbox.isConnected()) return;
  XboxControlsState value;
  uint32_t receivedMs = 0;
  bool available = false;
  portENTER_CRITICAL(&inputsState.xboxInputMux);
  if (inputsState.pendingXboxControlsAvailable) {
    value = inputsState.pendingXboxControls;
    receivedMs = inputsState.pendingXboxControlsMs;
    inputsState.pendingXboxControlsAvailable = false;
    available = true;
  }
  portEXIT_CRITICAL(&inputsState.xboxInputMux);
  if (!available) return;
  inputsState.xboxRawState = controlStateFromXboxControls(value, now);
  inputsState.xboxRawState.lastMs = receivedMs;
  inputsState.xboxState = calibratedControlState(inputsState.xboxRawState, configState.cfg.control.calibration);
  inputsState.xboxState.lastMs = receivedMs;

  const bool xboxArmPressed = configState.cfg.control.xboxArmEnabled && readButtonByName(inputsState.xboxState, configState.cfg.control.armButton);
  if (xboxArmPressed && !inputsState.lastXboxMenu) {
    toggleArm("xbox " + String(configState.cfg.control.armButton));
  }
  inputsState.lastXboxMenu = xboxArmPressed;
}

void initBle() {
  if (inputsState.bleReady) return;
  if (lifecycleState.optionalPeripheralsSkipped) {
    addLog("WARN", lifecycleState.optionalPeripheralSkipReason.length() ? lifecycleState.optionalPeripheralSkipReason : "BLE skipped by safe boot");
    return;
  }
  BLEGamepadClient::init(false);
  BLEGamepadClient::getAutoScan()->enable();
  BLEGamepadClient::getAutoScan()->onScanStarted([]() { addLog("INFO", "BLE scan started"); });
  BLEGamepadClient::getAutoScan()->onScanStopped([]() { addLog("INFO", "BLE scan stopped"); });
  inputsState.xbox.onConnected([](XboxController& controller) {
    (void)controller;
    addLog("INFO", "Xbox controller connected");
  });
  inputsState.xbox.onDisconnected([](XboxController& controller) {
    (void)controller;
    inputsState.pendingXboxDisconnected = true;
  });
  inputsState.xbox.onConnectionFailed([](XboxController& controller) {
    (void)controller;
    addLog("WARN", "Xbox connection failed");
  });
  inputsState.xbox.onValueChanged([](XboxControlsState& value) {
    publishXboxControlsFromBle(value);
  });
  inputsState.xbox.begin();
  inputsState.bleReady = true;
  addLog("INFO", "BLE gamepad client ready");
}

}  // namespace antcore_app
