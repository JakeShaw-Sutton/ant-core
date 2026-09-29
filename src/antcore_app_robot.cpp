#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/peripherals_state.h"
#include "app/robot_state.h"
#include "antcore_app_robot.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_inputs.h"
#include "antcore_app_log.h"
#include "antcore_app_peripherals.h"
#include "antcore_config.h"
#include <cmath>
#include "antcore_controls.h"
#include "antcore_output_test.h"
#include "antcore_outputs.h"
#include "antcore_safety.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

RobotState robotState;

static int clampInt(int value, int minValue, int maxValue);
static bool isFailsafeDisarmReason(const String& reason);
static void toggleWeaponArm(const String& source);
static void writeServoUs(uint8_t index, int us);
static void chooseActiveControl(uint32_t now, const antcore::WebControlSnapshot& web);
static void toggleDriveInvert(const String& source);
static void updateAutoDriveInvert();
static void triggerSelfRight(const String& source);
static void executeActionSlot(const char* action, const String& source);
static void updateMappedActionButtons(const ControlState& state);
static float computeGyroTurnCorrection(const ControlState& state, uint32_t now);
static float applyWeaponProfile(float target, uint32_t now);
static float computeWeaponTarget(const ControlState& state, uint32_t now);
static void computeMotorTargets(const ControlState& state, uint32_t now);
static void updateMotorOutputs(float dt);
static void computeServoOutputs(const ControlState& state, uint32_t now);

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

static bool isFailsafeDisarmReason(const String& reason) {
  return reason.indexOf("control") >= 0 || reason.indexOf("timeout") >= 0 ||
         reason.indexOf("disconnected") >= 0 ||
         reason.indexOf("battery critical") >= 0 || reason.indexOf("OTA") >= 0 ||
         reason.indexOf("pit mode") >= 0 || reason.indexOf("bench mode") >= 0 ||
         reason.indexOf("mapping test") >= 0;
}

void disarmRobot(const String& reason) {
  antcore_output_test::cancelOutputTests(robotState.outputTest);
  if (!robotState.armed && robotState.disarmReason == reason) return;
  const bool wasArmed = robotState.armed;
  robotState.armed = false;
  robotState.armedControlOwner = antcore::ArmedControlOwner();
  robotState.disarmReason = reason;
  robotState.weaponArmed = false;
  robotState.weaponToggle = false;
  robotState.weaponInputWasPressed = false;
  robotState.weaponProfileOut = 0.0f;
  robotState.lastWeaponProfileMs = 0;
  robotState.weaponDisarmReason = reason;
  robotState.selfRightActive = false;
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    robotState.motorTarget[i] = 0.0f;
  }
  stopAllMotors();
  disarmServos();
  robotState.lastGyroAssistMs = 0;
  robotState.gyroHeadingZ = 0.0f;
  robotState.gyroTargetZ = 0.0f;
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      robotState.servoButtonToggleLatched[i][b] = false;
      robotState.lastServoButtonPressed[i][b] = false;
    }
  }
  robotState.servoToggleSyncNeeded = true;
  if (wasArmed) {
    robotState.eventCounters.disarms++;
    if (isFailsafeDisarmReason(reason)) robotState.eventCounters.failsafes++;
  }
  addLog("INFO", "disarmed: " + reason);
}

bool canArm(String& reason, const String& source) {
  if (configState.cfg.security.authEnabled && antcore_config::isDefaultAdminPin(configState.cfg)) {
    reason = "set a non-default admin PIN before arming";
    return false;
  }
  String configError;
  const bool validConfig = configIsValid(&configError);
  antcore_safety::RobotArmSafetyInputs safety;
  safety.configValid = validConfig;
  safety.configError = configError.c_str();
  safety.pitMode = configState.cfg.safety.pitMode;
  safety.mappingTestMode = configState.cfg.control.calibration.mappingTestMode;
  safety.requireControlSource = configState.cfg.safety.requireControlSource;
  safety.freshControlSource = hasFreshControlSource();
  safety.serialSource = source.startsWith("serial");
  safety.liveOutputEnabled = robotState.outputTest.liveOutputEnabled;
  safety.otaInProgress = commandsState.otaInProgress;
  safety.benchMode = configState.cfg.battery.benchMode;
  safety.batteryCritical = ANTCORE_HAS_BATTERY_SENSE && configState.cfg.battery.enabled && peripheralsState.battery.critical;
  const antcore_safety::RobotArmDecision decision = antcore_safety::evaluateRobotArmSafety(safety);
  char reasonBuffer[128];
  antcore_safety::formatRobotArmDecisionReason(decision, configError.c_str(), reasonBuffer, sizeof(reasonBuffer));
  reason = reasonBuffer;
  return decision.allowed;
}

void armRobot(const String& source) {
  String reason;
  if (!canArm(reason, source)) {
    disarmRobot(reason);
    addLog("WARN", "arm blocked from " + source + ": " + reason);
    return;
  }
  if (!robotState.armed) {
    const auto web = webControlSnapshot();
    const uint32_t now = millis();
    robotState.armedControlOwner.source = antcore::selectControlSource(web, xboxControlFresh(now), now);
    robotState.armedControlOwner.webGeneration = web.generation;
    if (robotState.armedControlOwner.source == antcore::ControlSource::None && robotState.outputTest.liveOutputEnabled) {
      robotState.armedControlOwner.source = antcore::ControlSource::Test;
    }
    robotState.armed = true;
    robotState.disarmReason = "";
    addLog("INFO", "armed from " + source);
  }
}

void disarmWeapon(const String& reason) {
  if (!robotState.weaponArmed && robotState.weaponDisarmReason == reason) return;
  robotState.weaponArmed = false;
  robotState.weaponToggle = false;
  robotState.weaponProfileOut = 0.0f;
  robotState.lastWeaponProfileMs = 0;
  robotState.weaponDisarmReason = reason;
  if (!strcmp(configState.cfg.control.selfRight.target, "weapon")) {
    robotState.selfRightActive = false;
    robotState.selfRightUntilMs = 0;
  }
  if (configState.cfg.weapon.motor < MOTOR_COUNT) {
    robotState.motorTarget[configState.cfg.weapon.motor] = 0.0f;
    robotState.motorOut[configState.cfg.weapon.motor] = 0.0f;
    writeMotorRaw(configState.cfg.weapon.motor, 0.0f);
  }
  addLog("INFO", "weapon safe: " + reason);
}

void armWeapon(const String& source) {
  const antcore_safety::WeaponArmDecision decision =
      antcore_safety::evaluateWeaponArmSafety(robotState.armed, configState.cfg.weapon.enabled);
  if (!decision.allowed) {
    robotState.weaponDisarmReason = antcore_safety::weaponArmDecisionReason(decision.code);
    addLog("WARN", "weapon arm blocked from " + source + ": " +
                       String(antcore_safety::weaponArmLogReason(decision.code)));
    return;
  }
  if (!robotState.weaponArmed) {
    robotState.weaponArmed = true;
    robotState.weaponDisarmReason = "";
    robotState.eventCounters.weaponArms++;
    addLog("INFO", "weapon armed from " + source);
  }
}

static void toggleWeaponArm(const String& source) {
  if (robotState.weaponArmed) {
    disarmWeapon("remote weapon kill: " + source);
  } else {
    armWeapon(source);
  }
}

void toggleArm(const String& source) {
  if (robotState.armed) {
    disarmRobot("remote kill: " + source);
  } else {
    armRobot(source);
  }
}

uint16_t motorDutyForSide(float power, bool sideA) {
  power = clampFloat(power, -1.0f, 1.0f);
  if (fabsf(power) < 0.002f) return 0;
  if ((sideA && power > 0.0f) || (!sideA && power < 0.0f)) {
    return static_cast<uint16_t>(fabsf(power) * MOTOR_PWM_MAX);
  }
  return 0;
}

void stopAllMotors() {
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    robotState.motorTarget[i] = 0.0f;
    robotState.motorOut[i] = 0.0f;
  }
  stopAllMotorPins();
}



static void writeServoUs(uint8_t index, int us) {
  if (index >= SERVO_COUNT) return;
  writeServoUsOutput(index, clampInt(us, 500, 2500), robotState.servos, robotState.servoAttached, robotState.servoOutUs);
}

void disarmServos() {
  disarmServoOutputs(robotState.servos, robotState.servoAttached, robotState.servoOutUs, configState.cfg.servos);
}

static void chooseActiveControl(uint32_t now, const antcore::WebControlSnapshot& web) {
  const auto source = robotState.armed ? robotState.armedControlOwner.source : antcore::selectControlSource(web, xboxControlFresh(now), now);
  if (source == antcore::ControlSource::Web && web.fresh(now)) {
    robotState.activeState = web.frame;
  } else if (source == antcore::ControlSource::Xbox && xboxControlFresh(now)) {
    robotState.activeState = inputsState.xboxState;
  } else {
    robotState.activeState = ControlState();
  }
  if (source != robotState.lastSelectedSource ||
      (source == antcore::ControlSource::Web && web.generation != robotState.lastSelectedWebGeneration)) {
    // A handover is not a button press. Require release/press for mapped actions.
    const auto& state = robotState.activeState;
    robotState.lastDriveInvertButton = readButtonByName(state, configState.cfg.drive.invertButton);
    robotState.lastWeaponArmButton = readButtonByName(state, configState.cfg.weapon.armButton);
    for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
      robotState.lastActionSlotPressed[i] = readButtonByName(state, configState.cfg.control.actions[i].button);
    }
    robotState.servoToggleSyncNeeded = true;
    robotState.lastSelectedSource = source;
    robotState.lastSelectedWebGeneration = web.generation;
  }
}

static void toggleDriveInvert(const String& source) {
  robotState.driveInverted = !robotState.driveInverted;
  addLog("INFO", String(robotState.driveInverted ? "drive inverted: " : "drive normal: ") + source);
}

static void updateAutoDriveInvert() {
  robotState.autoDriveInverted = antcore::shouldAutoInvertDrive(configState.cfg.drive.autoInvertWithImu, configState.cfg.drive.invertible,
                                                     peripheralsState.imuTelemetry.present, peripheralsState.imuTelemetry.az,
                                                     configState.cfg.drive.autoInvertAzThreshold);
}

bool selfRightIsActive(uint32_t now) {
  robotState.selfRightActive = antcore::timedActionActive(now, robotState.selfRightUntilMs, robotState.selfRightActive);
  return robotState.selfRightActive;
}

static void triggerSelfRight(const String& source) {
  const uint32_t now = millis();
  if (!configState.cfg.control.selfRight.enabled) {
    addLog("WARN", "self-right blocked from " + source + ": disabled");
    return;
  }
  if (!robotState.armed) {
    addLog("WARN", "self-right blocked from " + source + ": robot disarmed");
    return;
  }
  if (configState.cfg.control.selfRight.requireWeaponArm && !strcmp(configState.cfg.control.selfRight.target, "weapon") && !robotState.weaponArmed) {
    addLog("WARN", "self-right blocked from " + source + ": weapon safe");
    return;
  }
  if (!antcore::cooldownReady(now, robotState.lastSelfRightMs, configState.cfg.control.selfRight.cooldownMs)) {
    addLog("WARN", "self-right blocked from " + source + ": cooldown");
    return;
  }
  robotState.selfRightActive = true;
  robotState.lastSelfRightMs = now;
  robotState.selfRightUntilMs = now + configState.cfg.control.selfRight.durationMs;
  addLog("WARN", "self-right triggered from " + source);
}

static void executeActionSlot(const char* action, const String& source) {
  if (!strcmp(action, "robotArmToggle")) {
    toggleArm(source);
  } else if (!strcmp(action, "robotDisarm")) {
    disarmRobot(source);
  } else if (!strcmp(action, "weaponArmToggle")) {
    toggleWeaponArm(source);
  } else if (!strcmp(action, "weaponSafe")) {
    disarmWeapon(source);
  } else if (!strcmp(action, "driveInvertToggle")) {
    toggleDriveInvert(source);
  } else if (!strcmp(action, "selfRight")) {
    triggerSelfRight(source);
  }
}

static void updateMappedActionButtons(const ControlState& state) {
  const bool invertPressed = configState.cfg.drive.invertible && readButtonByName(state, configState.cfg.drive.invertButton);
  if (invertPressed && !robotState.lastDriveInvertButton) {
    toggleDriveInvert("mapped button");
  }
  robotState.lastDriveInvertButton = invertPressed;

  const bool weaponArmPressed = configState.cfg.weapon.enabled && readButtonByName(state, configState.cfg.weapon.armButton);
  if (weaponArmPressed && !robotState.lastWeaponArmButton) {
    toggleWeaponArm("mapped button");
  }
  robotState.lastWeaponArmButton = weaponArmPressed;

  for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
    const bool pressed = configState.cfg.control.actions[i].enabled &&
                         readButtonByName(state, configState.cfg.control.actions[i].button);
    if (pressed && !robotState.lastActionSlotPressed[i]) {
      executeActionSlot(configState.cfg.control.actions[i].action, "action slot " + String(i + 1));
    }
    robotState.lastActionSlotPressed[i] = pressed;
  }
}

static float computeGyroTurnCorrection(const ControlState& state, uint32_t now) {
  if (!configState.cfg.drive.gyroAssist || !peripheralsState.imuTelemetry.present || strcmp(configState.cfg.drive.mode, "arcade") != 0) {
    robotState.lastGyroAssistMs = now;
    robotState.gyroTargetZ = robotState.gyroHeadingZ;
    return 0.0f;
  }
  const float dt = robotState.lastGyroAssistMs == 0 ? 0.0f : (now - robotState.lastGyroAssistMs) / 1000.0f;
  robotState.lastGyroAssistMs = now;
  robotState.gyroHeadingZ += peripheralsState.imuTelemetry.gz * dt;

  const float throttle = applyDeadbandExpo(readAnalogInputByName(state, configState.cfg.drive.throttleAxis),
                                           configState.cfg.drive.deadband, configState.cfg.drive.expo);
  const float turn = applyDeadbandExpo(readAnalogInputByName(state, configState.cfg.drive.turnAxis),
                                       configState.cfg.drive.deadband, configState.cfg.drive.expo);
  if (fabsf(throttle) < 0.08f || fabsf(turn) > 0.08f) {
    robotState.gyroTargetZ = robotState.gyroHeadingZ;
    return 0.0f;
  }
  return clampFloat((robotState.gyroTargetZ - robotState.gyroHeadingZ) * configState.cfg.drive.gyroGain, -0.35f, 0.35f);
}

static float applyWeaponProfile(float target, uint32_t now) {
  if (strcmp(configState.cfg.weapon.profile, "spinner") != 0) {
    robotState.weaponProfileOut = target;
    robotState.lastWeaponProfileMs = now;
    return target;
  }

  target = max(0.0f, target);
  const float dt = robotState.lastWeaponProfileMs == 0 ? 0.0f : max(0.0f, (now - robotState.lastWeaponProfileMs) / 1000.0f);
  robotState.lastWeaponProfileMs = now;
  const float rate = target > robotState.weaponProfileOut ? configState.cfg.weapon.rampUpPerSecond : configState.cfg.weapon.rampDownPerSecond;
  robotState.weaponProfileOut += clampFloat(target - robotState.weaponProfileOut, -rate * dt, rate * dt);
  return clampFloat(robotState.weaponProfileOut, 0.0f, configState.cfg.weapon.maxOutput);
}

static float computeWeaponTarget(const ControlState& state, uint32_t now) {
  if (!configState.cfg.weapon.enabled || configState.cfg.weapon.motor >= MOTOR_COUNT ||
      (configState.cfg.weapon.requireDedicatedArm && !robotState.weaponArmed)) {
    robotState.weaponProfileOut = 0.0f;
    robotState.lastWeaponProfileMs = now;
    return 0.0f;
  }
  const antcore::WeaponInputResult input = antcore::resolveWeaponInput(
      readAnalogInputByName(state, configState.cfg.weapon.input), readButtonByName(state, configState.cfg.weapon.input),
      configState.cfg.weapon.toggle, robotState.weaponToggle, robotState.weaponInputWasPressed, configState.cfg.weapon.buttonPower);
  robotState.weaponToggle = input.toggleLatched;
  robotState.weaponInputWasPressed = input.inputWasPressed;
  float value = input.value;
  if (configState.cfg.weapon.invert) value = -value;
  if (!strcmp(configState.cfg.weapon.profile, "spinner")) {
    value = max(0.0f, value);
  }
  return applyWeaponProfile(clampFloat(value, -configState.cfg.weapon.maxOutput, configState.cfg.weapon.maxOutput), now);
}

static void computeMotorTargets(const ControlState& state, uint32_t now) {
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    robotState.motorTarget[i] = 0.0f;
  }
  updateAutoDriveInvert();

  if (!robotState.armed || commandsState.otaInProgress) {
    return;
  }

  if (antcore_output_test::motorTestActive(robotState.outputTest, robotState.outputTest.motor, now)) {
    robotState.motorTarget[robotState.outputTest.motor] = robotState.outputTest.motorPower;
    return;
  }
  robotState.outputTest.motorActive = false;

  antcore::DriveInputs mixInputs;
  mixInputs.leftX = readAnalogInputByName(state, configState.cfg.drive.turnAxis);
  mixInputs.leftY = readAnalogInputByName(state, configState.cfg.drive.throttleAxis);
  mixInputs.rightY = readAnalogInputByName(state, configState.cfg.drive.rightTankAxis);
  if (!strcmp(configState.cfg.drive.mode, "tank")) {
    mixInputs.leftY = readAnalogInputByName(state, configState.cfg.drive.leftTankAxis);
  }
  mixInputs.turnCorrection = computeGyroTurnCorrection(state, now);
  mixInputs.precision = readButtonByName(state, configState.cfg.drive.precisionButton);
  mixInputs.turbo = readButtonByName(state, configState.cfg.drive.turboButton);
  mixInputs.derate = ANTCORE_HAS_BATTERY_SENSE && peripheralsState.battery.derating;

  antcore::DriveMixConfig mixConfig;
  mixConfig.tankMode = !strcmp(configState.cfg.drive.mode, "tank");
  mixConfig.driveInverted = robotState.driveInverted ^ robotState.autoDriveInverted;
  mixConfig.deadband = configState.cfg.drive.deadband;
  mixConfig.expo = configState.cfg.drive.expo;
  mixConfig.throttleScale = configState.cfg.drive.throttleScale;
  mixConfig.turnScale = configState.cfg.drive.turnScale;
  mixConfig.precisionScale = configState.cfg.drive.precisionScale;
  mixConfig.turboScale = configState.cfg.drive.turboScale;
  mixConfig.derateScale = configState.cfg.battery.derateScale;

  const antcore::DriveMixResult mix = antcore::computeDriveMix(mixInputs, mixConfig);
  const float left = mix.left;
  const float right = mix.right;

  robotState.motorTarget[configState.cfg.drive.leftMotor] = left;
  robotState.motorTarget[configState.cfg.drive.rightMotor] = right;
  if (configState.cfg.weapon.enabled) {
    robotState.motorTarget[configState.cfg.weapon.motor] = computeWeaponTarget(state, now);
  }
  if (selfRightIsActive(now) && !strcmp(configState.cfg.control.selfRight.target, "weapon") &&
      configState.cfg.weapon.enabled && configState.cfg.weapon.motor < MOTOR_COUNT && robotState.weaponArmed) {
    robotState.motorTarget[configState.cfg.weapon.motor] = configState.cfg.control.selfRight.motorPower;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    const bool weaponMotor = configState.cfg.weapon.enabled && configState.cfg.weapon.motor == i;
    robotState.motorTarget[i] = antcore::compensateMotor(robotState.motorTarget[i], weaponMotor ? 0.0f : configState.cfg.motors[i].trim,
                                           configState.cfg.motors[i].invert, configState.cfg.motors[i].maxOutput);
  }
}

static void updateMotorOutputs(float dt) {
  if (!robotState.armed || commandsState.otaInProgress) {
    stopAllMotors();
    return;
  }
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    float target = robotState.motorTarget[i];
    float ramp = configState.cfg.motors[i].rampPerSecond;
    if (ramp <= 0.0f) {
      robotState.motorOut[i] = target;
    } else {
      float step = ramp * dt;
      float delta = clampFloat(target - robotState.motorOut[i], -step, step);
      robotState.motorOut[i] += delta;
    }
    writeMotorRaw(i, robotState.motorOut[i]);
  }
}

static void computeServoOutputs(const ControlState& state, uint32_t now) {
  if (!robotState.armed || commandsState.otaInProgress) {
    disarmServos();
    robotState.servoToggleSyncNeeded = true;
    return;
  }
  if (antcore_output_test::servoTestActive(robotState.outputTest, robotState.outputTest.servo, now) &&
      configState.cfg.servos[robotState.outputTest.servo].enabled) {
    writeServoUs(robotState.outputTest.servo, robotState.outputTest.servoUs);
  } else {
    robotState.outputTest.servoActive = false;
  }

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    if (!configState.cfg.servos[i].enabled) continue;
    if (robotState.outputTest.servoActive && robotState.outputTest.servo == i) continue;
    int us = configState.cfg.servos[i].neutralUs;
    if (configState.cfg.servos[i].axisEnabled) {
      float axis = applyDeadbandExpo(readAnalogInputByName(state, configState.cfg.servos[i].axis), configState.cfg.drive.deadband, configState.cfg.drive.expo);
      if (configState.cfg.servos[i].invert) axis = -axis;
      if (axis >= 0.0f) {
        us = configState.cfg.servos[i].neutralUs + static_cast<int>((configState.cfg.servos[i].maxUs - configState.cfg.servos[i].neutralUs) * axis);
      } else {
        us = configState.cfg.servos[i].neutralUs + static_cast<int>((configState.cfg.servos[i].neutralUs - configState.cfg.servos[i].minUs) * axis);
      }
    }
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      if (!configState.cfg.servos[i].buttons[b].enabled) {
        robotState.servoButtonToggleLatched[i][b] = false;
        robotState.lastServoButtonPressed[i][b] = false;
        continue;
      }
      const bool pressed = readButtonByName(state, configState.cfg.servos[i].buttons[b].button);
      if (robotState.servoToggleSyncNeeded) {
        robotState.lastServoButtonPressed[i][b] = pressed;
      } else if (configState.cfg.servos[i].buttons[b].toggle) {
        if (pressed && !robotState.lastServoButtonPressed[i][b]) {
          robotState.servoButtonToggleLatched[i][b] = !robotState.servoButtonToggleLatched[i][b];
        }
        robotState.lastServoButtonPressed[i][b] = pressed;
      } else {
        robotState.servoButtonToggleLatched[i][b] = false;
        robotState.lastServoButtonPressed[i][b] = pressed;
      }
      if ((configState.cfg.servos[i].buttons[b].toggle && robotState.servoButtonToggleLatched[i][b]) ||
          (!configState.cfg.servos[i].buttons[b].toggle && pressed)) {
        us = configState.cfg.servos[i].buttons[b].us;
      }
    }
    if (selfRightIsActive(now) &&
        ((!strcmp(configState.cfg.control.selfRight.target, "servo1") && i == 0) ||
         (!strcmp(configState.cfg.control.selfRight.target, "servo2") && i == 1))) {
      us = configState.cfg.control.selfRight.servoUs;
    }
    writeServoUs(i, us);
  }
  robotState.servoToggleSyncNeeded = false;
}

void updateOutputs() {
  updateXboxStateFromController(millis());
  const auto web = webControlSnapshot();
  // Sample time after consuming mailboxes; a just-arrived frame must not appear
  // to be in the future and underflow unsigned age calculations.
  const uint32_t now = millis();
  const float dt = max(0.001f, (now - robotState.lastControlMs) / 1000.0f);
  robotState.lastControlMs = now;

  antcore_safety::RuntimeSafetyInputs runtimeSafety;
  runtimeSafety.armed = robotState.armed;
  runtimeSafety.pitMode = configState.cfg.safety.pitMode;
  runtimeSafety.mappingTestMode = configState.cfg.control.calibration.mappingTestMode;
  runtimeSafety.benchMode = configState.cfg.battery.benchMode;
  runtimeSafety.xboxConnected = inputsState.bleReady && inputsState.xbox.isConnected();
  runtimeSafety.xboxInputSeen = inputsState.xboxState.lastMs != 0;
  runtimeSafety.requireControlSource = configState.cfg.safety.requireControlSource;
  runtimeSafety.owner = robotState.armedControlOwner;
  runtimeSafety.webDriverActive = web.locked(now);
  runtimeSafety.webGeneration = web.generation;
  runtimeSafety.webInputSeen = web.inputSeen;
  runtimeSafety.webInputAgeMs = now - web.frame.lastMs;
  runtimeSafety.xboxInputAgeMs = now - inputsState.xboxState.lastMs;
  const char* runtimeReason = antcore_safety::runtimeDisarmReason(runtimeSafety);
  if (runtimeReason[0] != '\0') {
    disarmRobot(runtimeReason);
    // Do not execute another controller's held arm/action buttons on this tick.
    robotState.activeState = ControlState();
    return;
  }

  chooseActiveControl(now, web);
  const bool staleOwner = robotState.armed &&
      ((robotState.armedControlOwner.source == antcore::ControlSource::Web && !web.fresh(now)) ||
       (robotState.armedControlOwner.source == antcore::ControlSource::Xbox && !xboxControlFresh(now)));
  if (staleOwner) {
    stopAllMotors();
    disarmServos();
    robotState.weaponToggle = false;
    robotState.weaponProfileOut = 0.0f;
    antcore_output_test::cancelOutputTests(robotState.outputTest);
    return;
  }
  updateMappedActionButtons(robotState.activeState);
  computeMotorTargets(robotState.activeState, now);
  computeServoOutputs(robotState.activeState, now);
  updateMotorOutputs(dt);
}

void initPins() {
  initMotorOutputs();
  stopAllMotors();
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    robotState.servoOutUs[i] = configState.cfg.servos[i].failsafeUs;
  }
  disarmServos();
}

}  // namespace antcore_app
