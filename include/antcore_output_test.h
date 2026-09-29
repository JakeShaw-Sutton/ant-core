#pragma once

#include <cstdint>

#include "antcore_firmware_config.h"

namespace antcore_output_test {

struct GateResult {
  bool ok = false;
  const char* reason = "";
};

struct QueueResult {
  bool ok = false;
  const char* reason = "";
  int index = 0;
};

void disableLiveOutput(OutputTest& state);
void cancelOutputTests(OutputTest& state);
GateResult setLiveOutputEnabled(OutputTest& state, bool enabled, bool pitMode, bool mappingTestMode);
QueueResult queueMotorTest(OutputTest& state, bool armed, bool pitMode, bool mappingTestMode,
                           int requestedMotor, float power, int durationMs, uint32_t nowMs);
QueueResult queueServoTest(OutputTest& state, bool armed, bool pitMode, bool mappingTestMode,
                           const ServoConfig servoConfigs[], int requestedServo, int pulseUs,
                           int durationMs, uint32_t nowMs);
bool motorTestActive(const OutputTest& state, uint8_t motor, uint32_t nowMs);
bool servoTestActive(const OutputTest& state, uint8_t servo, uint32_t nowMs);

}  // namespace antcore_output_test
