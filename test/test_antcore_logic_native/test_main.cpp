#include <unity.h>
#include <initializer_list>

#include "antcore_controls.h"
#include "antcore_logic.h"
#include "antcore_output_test.h"
#include "antcore_safety.h"

using namespace antcore;

void setUp() {}

void tearDown() {}

void test_deadband_expo_and_drive_mix() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, clampFloat(2.0f, -1.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, applyDeadbandExpo(0.02f, 0.07f, 0.25f));

  DriveInputs arcade;
  arcade.leftY = 1.0f;
  arcade.leftX = 0.5f;
  DriveMixConfig drive;
  drive.deadband = 0.0f;
  drive.expo = 0.0f;
  DriveMixResult mixed = computeDriveMix(arcade, drive);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 1.0f, mixed.left);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.333333f, mixed.right);

  arcade.turnCorrection = -0.25f;
  mixed = computeDriveMix(arcade, drive);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 1.0f, mixed.left);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.6f, mixed.right);

  arcade.turnCorrection = 0.0f;
  drive.driveInverted = true;
  mixed = computeDriveMix(arcade, drive);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, -0.333333f, mixed.left);
  TEST_ASSERT_FLOAT_WITHIN(0.002f, -1.0f, mixed.right);
}

void test_tank_precision_derate_and_battery_scaling() {
  DriveInputs tank;
  tank.leftY = 0.8f;
  tank.rightY = -0.4f;
  tank.precision = true;
  tank.derate = true;
  DriveMixConfig drive;
  drive.tankMode = true;
  drive.deadband = 0.0f;
  drive.expo = 0.0f;
  drive.precisionScale = 0.5f;
  drive.derateScale = 0.7f;
  DriveMixResult mixed = computeDriveMix(tank, drive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, mixed.left);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.2f, mixed.right);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, mixed.scale);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.5f, scaleBatteryVoltage(2.5f, 3.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.25f, scaleBatteryVoltage(2.5f, 3.0f, 1.1f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, scaleBatteryVoltage(-1.0f, 3.0f, 1.0f));
}

void test_battery_safety_and_arm_blocks() {
  BatterySafetyResult battery = evaluateBatterySafety(6.6f, true, 7.0f, 6.4f, true, 6.8f);
  TEST_ASSERT_TRUE(battery.warn);
  TEST_ASSERT_FALSE(battery.critical);
  TEST_ASSERT_TRUE(battery.derating);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.3f, battery.cellVolts);

  battery = evaluateBatterySafety(6.2f, true, 7.0f, 6.4f, true, 6.8f);
  TEST_ASSERT_TRUE(battery.critical);
  TEST_ASSERT_FALSE(battery.derating);

  TEST_ASSERT_EQUAL(ArmBlockReason::OtaActive,
                    evaluateArmBlock(true, false, false, true, false, false));
  TEST_ASSERT_EQUAL(ArmBlockReason::BenchMode,
                    evaluateArmBlock(false, true, false, true, false, false));
  TEST_ASSERT_EQUAL(ArmBlockReason::BatteryCritical,
                    evaluateArmBlock(false, false, true, true, false, false));
  TEST_ASSERT_EQUAL(ArmBlockReason::NoControlSource,
                    evaluateArmBlock(false, false, false, false, false, false));
  TEST_ASSERT_EQUAL(ArmBlockReason::NoControlSource,
                    evaluateArmBlock(false, false, false, false, true, false));
  TEST_ASSERT_EQUAL(ArmBlockReason::None,
                    evaluateArmBlock(false, false, false, false, false, true));
  TEST_ASSERT_EQUAL_STRING("battery critical", armBlockReasonText(ArmBlockReason::BatteryCritical));
}

void test_axis_calibration_auto_invert_and_timed_actions() {
  AxisCalibration cal;
  cal.minValue = -0.8f;
  cal.centerValue = 0.1f;
  cal.maxValue = 0.9f;
  cal.deadband = 0.05f;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, applyAxisCalibration(0.1f, cal, false));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, applyAxisCalibration(0.9f, cal, false));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, applyAxisCalibration(-0.8f, cal, false));
  cal.invert = true;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, applyAxisCalibration(0.9f, cal, false));

  AxisCalibration trigger;
  trigger.minValue = 0.0f;
  trigger.centerValue = 0.0f;
  trigger.maxValue = 1.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.484536f, applyAxisCalibration(0.5f, trigger, true));

  TEST_ASSERT_TRUE(shouldAutoInvertDrive(true, true, true, -0.55f, -0.45f));
  TEST_ASSERT_FALSE(shouldAutoInvertDrive(true, true, false, -0.55f, -0.45f));
  TEST_ASSERT_FALSE(shouldAutoInvertDrive(true, false, true, -0.55f, -0.45f));
  TEST_ASSERT_FALSE(shouldAutoInvertDrive(true, true, true, -0.10f, -0.45f));

  TEST_ASSERT_TRUE(timedActionActive(1000, 1200, true));
  TEST_ASSERT_FALSE(timedActionActive(1200, 1200, true));
  TEST_ASSERT_TRUE(cooldownReady(1000, 0, 1500));
  TEST_ASSERT_FALSE(cooldownReady(1200, 1000, 1500));
  TEST_ASSERT_EQUAL_UINT32(1300, cooldownRemainingMs(1200, 1000, 1500));
}

void test_weapon_input_resolution_for_buttons_triggers_and_toggles() {
  WeaponInputResult input = resolveWeaponInput(0.35f, false, false, false, false, 0.8f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.35f, input.value);
  TEST_ASSERT_FALSE(input.toggleLatched);
  TEST_ASSERT_FALSE(input.inputWasPressed);

  input = resolveWeaponInput(0.0f, true, false, false, false, 0.8f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, input.value);
  TEST_ASSERT_TRUE(input.inputWasPressed);

  input = resolveWeaponInput(0.0f, true, true, false, false, 0.75f);
  TEST_ASSERT_TRUE(input.toggleLatched);
  TEST_ASSERT_TRUE(input.inputWasPressed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, input.value);

  input = resolveWeaponInput(0.0f, true, true, input.toggleLatched, input.inputWasPressed, 0.75f);
  TEST_ASSERT_TRUE(input.toggleLatched);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, input.value);

  input = resolveWeaponInput(0.0f, false, true, input.toggleLatched, input.inputWasPressed, 0.75f);
  TEST_ASSERT_TRUE(input.toggleLatched);
  TEST_ASSERT_FALSE(input.inputWasPressed);

  input = resolveWeaponInput(0.0f, true, true, input.toggleLatched, input.inputWasPressed, 0.75f);
  TEST_ASSERT_FALSE(input.toggleLatched);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, input.value);

  input = resolveWeaponInput(0.7f, false, true, false, false, 0.9f);
  TEST_ASSERT_TRUE(input.toggleLatched);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, input.value);
}

void test_controller_lookup_and_calibration_helpers() {
  ControlState state;
  state.a = true;
  state.leftY = -0.5f;
  state.rightTrigger = 0.8f;

  TEST_ASSERT_TRUE(isKnownAxisName("leftY"));
  TEST_ASSERT_TRUE(isKnownButtonName("a"));
  TEST_ASSERT_TRUE(isKnownInputName("rightTrigger"));
  TEST_ASSERT_TRUE(isKnownInputName("rightBumper"));
  TEST_ASSERT_TRUE(isKnownActionName("weaponSafe"));
  TEST_ASSERT_FALSE(isKnownInputName("notReal"));
  TEST_ASSERT_EQUAL_INT8(5, axisIndexByName("rightTrigger"));
  TEST_ASSERT_TRUE(axisIsPositiveOnly(4));
  TEST_ASSERT_TRUE(readButtonByName(state, "a"));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, readAnalogInputByName(state, "leftY"));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, readAnalogInputByName(state, "rightTrigger"));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, readAnalogInputByName(state, "a"));

  AxisCalibration triggerCalibration;
  triggerCalibration.minValue = 0.9f;
  triggerCalibration.centerValue = 0.8f;
  triggerCalibration.maxValue = 0.81f;
  sanitizeAxisCalibration(triggerCalibration, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, triggerCalibration.minValue);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, triggerCalibration.centerValue);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, triggerCalibration.maxValue);

  ControlConfig::CalibrationConfig calibrationConfig;
  calibrationConfig.axes[1].invert = true;
  ControlState raw;
  raw.leftY = 1.0f;
  ControlState calibrated = calibratedControlState(raw, calibrationConfig);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, calibrated.leftY);

  calibrationConfig.enabled = false;
  calibrated = calibratedControlState(raw, calibrationConfig);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibrated.leftY);
}

void test_safety_module_decisions_preserve_failsafe_ordering() {
  antcore_safety::RobotArmSafetyInputs arm;
  arm.configValid = false;
  arm.configError = "weapon motor overlaps a drive motor";
  antcore_safety::RobotArmDecision decision = antcore_safety::evaluateRobotArmSafety(arm);
  TEST_ASSERT_FALSE(decision.allowed);
  TEST_ASSERT_EQUAL(antcore_safety::RobotArmDecisionCode::ConfigInvalid, decision.code);
  char reason[96];
  antcore_safety::formatRobotArmDecisionReason(decision, arm.configError, reason, sizeof(reason));
  TEST_ASSERT_EQUAL_STRING("config invalid: weapon motor overlaps a drive motor", reason);

  arm.configValid = true;
  arm.pitMode = true;
  decision = antcore_safety::evaluateRobotArmSafety(arm);
  TEST_ASSERT_EQUAL(antcore_safety::RobotArmDecisionCode::PitMode, decision.code);

  arm.pitMode = false;
  arm.batteryCritical = true;
  decision = antcore_safety::evaluateRobotArmSafety(arm);
  TEST_ASSERT_EQUAL(antcore_safety::RobotArmDecisionCode::BatteryCritical, decision.code);

  arm.batteryCritical = false;
  arm.freshControlSource = true;
  decision = antcore_safety::evaluateRobotArmSafety(arm);
  TEST_ASSERT_TRUE(decision.allowed);

  antcore_safety::RuntimeSafetyInputs runtime;
  runtime.armed = true;
  runtime.pitMode = true;
  TEST_ASSERT_EQUAL_STRING("pit mode active", antcore_safety::runtimeDisarmReason(runtime));

  runtime.pitMode = false;
  runtime.xboxConnected = false;
  runtime.webInputAgeMs = WEB_CONTROL_DISARM_MS + 1;
  runtime.xboxInputAgeMs = XBOX_CONTROL_STALE_MS + 1;
  TEST_ASSERT_EQUAL_STRING("control link lost", antcore_safety::runtimeDisarmReason(runtime));

  runtime.owner.source = ControlSource::Xbox;
  runtime.xboxConnected = true;
  runtime.xboxInputSeen = true;
  runtime.xboxInputAgeMs = 1;
  TEST_ASSERT_EQUAL_STRING("", antcore_safety::runtimeDisarmReason(runtime));
  runtime.xboxConnected = false;
  TEST_ASSERT_EQUAL_STRING("Xbox disconnected", antcore_safety::runtimeDisarmReason(runtime));
  runtime.xboxConnected = true;
  runtime.xboxInputAgeMs = XBOX_CONTROL_DISARM_MS + 1;
  TEST_ASSERT_EQUAL_STRING("Xbox control timeout", antcore_safety::runtimeDisarmReason(runtime));

  runtime.owner.source = ControlSource::Web;
  runtime.webDriverActive = true;
  runtime.webInputSeen = true;
  runtime.webInputAgeMs = 1;
  TEST_ASSERT_EQUAL_STRING("", antcore_safety::runtimeDisarmReason(runtime));
  runtime.webInputAgeMs = WEB_CONTROL_DISARM_MS + 1;
  runtime.xboxInputAgeMs = 1;
  TEST_ASSERT_EQUAL_STRING("web control timeout", antcore_safety::runtimeDisarmReason(runtime));
  // Disabling the source requirement never masks loss of an existing owner.
  runtime.requireControlSource = false;
  TEST_ASSERT_EQUAL_STRING("web control timeout", antcore_safety::runtimeDisarmReason(runtime));
  runtime.webInputAgeMs = 1;
  runtime.webGeneration++;
  TEST_ASSERT_EQUAL_STRING("web control link lost", antcore_safety::runtimeDisarmReason(runtime));
  runtime.owner.source = ControlSource::None;
  TEST_ASSERT_EQUAL_STRING("", antcore_safety::runtimeDisarmReason(runtime));

  antcore_safety::WeaponArmDecision weapon =
      antcore_safety::evaluateWeaponArmSafety(false, true);
  TEST_ASSERT_FALSE(weapon.allowed);
  TEST_ASSERT_EQUAL(antcore_safety::WeaponArmDecisionCode::RobotDisarmed, weapon.code);
  TEST_ASSERT_EQUAL_STRING("robot disarmed", antcore_safety::weaponArmDecisionReason(weapon.code));

  weapon = antcore_safety::evaluateWeaponArmSafety(true, false);
  TEST_ASSERT_FALSE(weapon.allowed);
  TEST_ASSERT_EQUAL_STRING("weapon disabled", antcore_safety::weaponArmDecisionReason(weapon.code));
  TEST_ASSERT_EQUAL_STRING("disabled", antcore_safety::weaponArmLogReason(weapon.code));
}

void test_output_test_module_gates_live_pulses() {
  OutputTest output;
  ServoConfig servoConfigs[SERVO_COUNT];
  antcore_output_test::GateResult gate =
      antcore_output_test::setLiveOutputEnabled(output, true, true, false);
  TEST_ASSERT_FALSE(gate.ok);
  TEST_ASSERT_EQUAL_STRING("pit mode active", gate.reason);
  TEST_ASSERT_FALSE(output.liveOutputEnabled);

  gate = antcore_output_test::setLiveOutputEnabled(output, true, false, true);
  TEST_ASSERT_FALSE(gate.ok);
  TEST_ASSERT_EQUAL_STRING("controller mapping test mode active", gate.reason);

  gate = antcore_output_test::setLiveOutputEnabled(output, true, false, false);
  TEST_ASSERT_TRUE(gate.ok);
  TEST_ASSERT_TRUE(output.liveOutputEnabled);

  antcore_output_test::QueueResult queued =
      antcore_output_test::queueMotorTest(output, false, false, false, 1, 0.5f, 250, 1000);
  TEST_ASSERT_FALSE(queued.ok);
  TEST_ASSERT_EQUAL_STRING("robot must be armed", queued.reason);

  queued = antcore_output_test::queueMotorTest(output, true, false, false, 4, 0.5f, 250, 1000);
  TEST_ASSERT_FALSE(queued.ok);
  TEST_ASSERT_EQUAL_STRING("motor index out of range", queued.reason);

  queued = antcore_output_test::queueMotorTest(output, true, false, false, 2, 2.0f, 4000, 1000);
  TEST_ASSERT_TRUE(queued.ok);
  TEST_ASSERT_EQUAL_UINT8(1, output.motor);
  TEST_ASSERT_TRUE(output.motorActive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, output.motorPower);
  TEST_ASSERT_EQUAL_UINT32(3000, output.motorUntilMs);

  queued = antcore_output_test::queueServoTest(output, true, false, false, servoConfigs, 1, 400, 0, 2000);
  TEST_ASSERT_TRUE(queued.ok);
  TEST_ASSERT_EQUAL_UINT8(0, output.servo);
  TEST_ASSERT_TRUE(output.servoActive);
  TEST_ASSERT_EQUAL_INT(500, output.servoUs);
  TEST_ASSERT_EQUAL_UINT32(2001, output.servoUntilMs);
  TEST_ASSERT_EQUAL_UINT32(3000, output.motorUntilMs);

  servoConfigs[1].enabled = false;
  queued = antcore_output_test::queueServoTest(output, true, false, false, servoConfigs, 2, 1500, 120, 2100);
  TEST_ASSERT_FALSE(queued.ok);
  TEST_ASSERT_EQUAL_STRING("servo disabled", queued.reason);

  antcore_output_test::disableLiveOutput(output);
  TEST_ASSERT_FALSE(output.liveOutputEnabled);
  TEST_ASSERT_FALSE(output.motorActive);
  TEST_ASSERT_FALSE(output.servoActive);
  TEST_ASSERT_EQUAL_UINT32(0, output.motorUntilMs);
  TEST_ASSERT_EQUAL_UINT32(0, output.servoUntilMs);
}

void test_tank_inversion_and_trim_preserve_neutral_and_steering() {
  DriveMixConfig cfg;
  cfg.tankMode = true;
  cfg.deadband = 0;
  cfg.expo = 0;
  DriveInputs input;
  input.leftY = 0.8f;
  input.rightY = 0.4f;
  cfg.driveInverted = true;
  auto result = computeDriveMix(input, cfg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.4f, result.left);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, result.right);
  input.rightY = -input.leftY;
  result = computeDriveMix(input, cfg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, result.left);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.8f, result.right);
  for (float trim : {-0.25f, 0.0f, 0.25f}) {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, compensateMotor(0, trim, false, 1));
    TEST_ASSERT_TRUE(compensateMotor(-0.05f, trim, false, 1) < 0);
    TEST_ASSERT_TRUE(compensateMotor(0.05f, trim, false, 1) > 0);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.55f, compensateMotor(0.5f, 0.1f, true, 1));
  TEST_ASSERT_EQUAL_FLOAT(0.4f, compensateMotor(1, 0.25f, false, 0.4f));
}

void test_web_ownership_is_connection_bound_and_rollover_safe() {
  WebControlOwner owner;
  const uint32_t now = UINT32_MAX - 100;
  TEST_ASSERT_TRUE(owner.claim(10, "same-tab-id", now));
  const auto generation = owner.snapshot().generation;
  TEST_ASSERT_FALSE(owner.claim(11, "same-tab-id", now + 1));
  ControlState frame;
  frame.leftY = 0.75f;
  TEST_ASSERT_FALSE(owner.publish(11, "same-tab-id", frame, now + 2));
  TEST_ASSERT_FALSE(owner.release(11));
  TEST_ASSERT_TRUE(owner.publish(10, "same-tab-id", frame, now + 2));
  TEST_ASSERT_TRUE(owner.snapshot().fresh(10));
  TEST_ASSERT_FALSE(owner.snapshot().fresh(1000));
  TEST_ASSERT_TRUE(owner.claim(10, "same-tab-id", 1000));
  // A claim heartbeat cannot refresh the age of the actual control frame.
  TEST_ASSERT_FALSE(owner.snapshot().fresh(1000));
  TEST_ASSERT_TRUE(owner.release(10));
  TEST_ASSERT_TRUE(owner.claim(11, "same-tab-id", 1001));
  TEST_ASSERT_FALSE(owner.snapshot().inputSeen);
  TEST_ASSERT_NOT_EQUAL(generation, owner.snapshot().generation);
  TEST_ASSERT_FALSE(owner.publish(10, "same-tab-id", frame, 1002));
  TEST_ASSERT_EQUAL(ControlSource::None, selectControlSource(owner.snapshot(), true, 1002));
  TEST_ASSERT_TRUE(owner.publish(11, "same-tab-id", frame, 1002));
  TEST_ASSERT_EQUAL(ControlSource::Web, selectControlSource(owner.snapshot(), true, 1003));
}

void test_disarm_cancellation_preserves_enable_but_never_resumes_a_pulse() {
  OutputTest tests;
  tests.liveOutputEnabled = true;
  ServoConfig servos[SERVO_COUNT];
  TEST_ASSERT_TRUE(antcore_output_test::queueMotorTest(tests, true, false, false, 1, 0.5f, 2000, 100).ok);
  TEST_ASSERT_TRUE(antcore_output_test::queueServoTest(tests, true, false, false, servos, 1, 1800, 2000, 100).ok);
  antcore_output_test::cancelOutputTests(tests);
  TEST_ASSERT_TRUE(tests.liveOutputEnabled);
  TEST_ASSERT_FALSE(antcore_output_test::motorTestActive(tests, 0, 200));
  TEST_ASSERT_FALSE(antcore_output_test::servoTestActive(tests, 0, 200));
  antcore_output_test::cancelOutputTests(tests);
  TEST_ASSERT_EQUAL_UINT32(0, tests.motorUntilMs);
  TEST_ASSERT_TRUE(antcore_output_test::queueMotorTest(tests, true, false, false, 1, 0.5f, 100, 200).ok);
  TEST_ASSERT_TRUE(antcore_output_test::motorTestActive(tests, 0, 201));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deadband_expo_and_drive_mix);
  RUN_TEST(test_tank_precision_derate_and_battery_scaling);
  RUN_TEST(test_battery_safety_and_arm_blocks);
  RUN_TEST(test_axis_calibration_auto_invert_and_timed_actions);
  RUN_TEST(test_weapon_input_resolution_for_buttons_triggers_and_toggles);
  RUN_TEST(test_controller_lookup_and_calibration_helpers);
  RUN_TEST(test_safety_module_decisions_preserve_failsafe_ordering);
  RUN_TEST(test_output_test_module_gates_live_pulses);
  RUN_TEST(test_tank_inversion_and_trim_preserve_neutral_and_steering);
  RUN_TEST(test_web_ownership_is_connection_bound_and_rollover_safe);
  RUN_TEST(test_disarm_cancellation_preserves_enable_but_never_resumes_a_pulse);
  return UNITY_END();
}
