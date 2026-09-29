#include "antcore_output_test.h"

namespace antcore_output_test {

namespace {

constexpr int MIN_TEST_DURATION_MS = 1;
constexpr int MAX_TEST_DURATION_MS = 2000;
constexpr int MIN_SERVO_TEST_US = 500;
constexpr int MAX_SERVO_TEST_US = 2500;

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

GateResult gate(bool ok, const char* reason) {
  GateResult result;
  result.ok = ok;
  result.reason = reason;
  return result;
}

QueueResult queue(bool ok, const char* reason, int index = 0) {
  QueueResult result;
  result.ok = ok;
  result.reason = reason;
  result.index = index;
  return result;
}

const char* safetyBlockReason(bool pitMode, bool mappingTestMode) {
  if (pitMode) return "pit mode active";
  if (mappingTestMode) return "controller mapping test mode active";
  return "";
}

GateResult requireOutputSafe(bool pitMode, bool mappingTestMode) {
  const char* reason = safetyBlockReason(pitMode, mappingTestMode);
  if (reason[0] != '\0') return gate(false, reason);
  return gate(true, "");
}

QueueResult requireQueueAllowed(const OutputTest& state, bool armed, bool pitMode, bool mappingTestMode) {
  GateResult safe = requireOutputSafe(pitMode, mappingTestMode);
  if (!safe.ok) return queue(false, safe.reason);
  if (!state.liveOutputEnabled) return queue(false, "live output tests disabled");
  if (!armed) return queue(false, "robot must be armed");
  return queue(true, "");
}

}  // namespace

void cancelOutputTests(OutputTest& state) {
  state.motorActive = false;
  state.servoActive = false;
  state.motorPower = 0.0f;
  state.motorUntilMs = 0;
  state.servoUntilMs = 0;
}

void disableLiveOutput(OutputTest& state) {
  state.liveOutputEnabled = false;
  cancelOutputTests(state);
}

GateResult setLiveOutputEnabled(OutputTest& state, bool enabled, bool pitMode, bool mappingTestMode) {
  if (enabled) {
    GateResult safe = requireOutputSafe(pitMode, mappingTestMode);
    if (!safe.ok) {
      disableLiveOutput(state);
      return safe;
    }
  }
  state.liveOutputEnabled = enabled;
  if (!enabled) {
    cancelOutputTests(state);
  }
  return gate(true, "");
}

QueueResult queueMotorTest(OutputTest& state, bool armed, bool pitMode, bool mappingTestMode,
                           int requestedMotor, float power, int durationMs, uint32_t nowMs) {
  QueueResult allowed = requireQueueAllowed(state, armed, pitMode, mappingTestMode);
  if (!allowed.ok) return allowed;
  if (requestedMotor < 1 || requestedMotor > MOTOR_COUNT) {
    return queue(false, "motor index out of range", requestedMotor);
  }
  state.motor = static_cast<uint8_t>(requestedMotor - 1);
  state.motorPower = clampFloat(power, -1.0f, 1.0f);
  state.motorUntilMs = nowMs + static_cast<uint32_t>(clampInt(durationMs, MIN_TEST_DURATION_MS, MAX_TEST_DURATION_MS));
  state.motorActive = true;
  return queue(true, "", requestedMotor);
}

QueueResult queueServoTest(OutputTest& state, bool armed, bool pitMode, bool mappingTestMode,
                           const ServoConfig servoConfigs[], int requestedServo, int pulseUs,
                           int durationMs, uint32_t nowMs) {
  QueueResult allowed = requireQueueAllowed(state, armed, pitMode, mappingTestMode);
  if (!allowed.ok) return allowed;
  if (requestedServo < 1 || requestedServo > SERVO_COUNT) {
    return queue(false, "servo index out of range", requestedServo);
  }
  if (servoConfigs == nullptr || !servoConfigs[requestedServo - 1].enabled) {
    return queue(false, "servo disabled", requestedServo);
  }
  state.servo = static_cast<uint8_t>(requestedServo - 1);
  state.servoUs = clampInt(pulseUs, MIN_SERVO_TEST_US, MAX_SERVO_TEST_US);
  state.servoUntilMs = nowMs + static_cast<uint32_t>(clampInt(durationMs, MIN_TEST_DURATION_MS, MAX_TEST_DURATION_MS));
  state.servoActive = true;
  return queue(true, "", requestedServo);
}

bool motorTestActive(const OutputTest& state, uint8_t motor, uint32_t nowMs) {
  return state.liveOutputEnabled && state.motorActive && state.motor == motor &&
         static_cast<int32_t>(nowMs - state.motorUntilMs) < 0;
}

bool servoTestActive(const OutputTest& state, uint8_t servo, uint32_t nowMs) {
  return state.liveOutputEnabled && state.servoActive && state.servo == servo &&
         static_cast<int32_t>(nowMs - state.servoUntilMs) < 0;
}

}  // namespace antcore_output_test
