#pragma once

#include <cstdint>

namespace antcore {

struct DriveInputs {
  float leftX = 0.0f;
  float leftY = 0.0f;
  float rightY = 0.0f;
  float turnCorrection = 0.0f;
  bool precision = false;
  bool turbo = false;
  bool derate = false;
};

struct DriveMixConfig {
  bool tankMode = false;
  bool driveInverted = false;
  float deadband = 0.07f;
  float expo = 0.25f;
  float throttleScale = 1.0f;
  float turnScale = 1.0f;
  float precisionScale = 0.45f;
  float turboScale = 1.0f;
  float derateScale = 0.70f;
};

struct DriveMixResult {
  float left = 0.0f;
  float right = 0.0f;
  float scale = 1.0f;
};

struct BatterySafetyResult {
  float cellVolts = 0.0f;
  bool warn = false;
  bool critical = false;
  bool derating = false;
};

struct AxisCalibration {
  float minValue = -1.0f;
  float centerValue = 0.0f;
  float maxValue = 1.0f;
  float deadband = 0.03f;
  bool invert = false;
};

struct WeaponInputResult {
  float value = 0.0f;
  bool toggleLatched = false;
  bool inputWasPressed = false;
};

enum class ArmBlockReason {
  None,
  OtaActive,
  BenchMode,
  BatteryCritical,
  NoControlSource,
};

float clampFloat(float value, float minValue, float maxValue);
// Trim is proportional gain compensation: zero and direction are preserved.
float compensateMotor(float target, float trim, bool invert, float maxOutput);
float applyDeadbandExpo(float value, float deadband, float expo);
DriveMixResult computeDriveMix(const DriveInputs& inputs, const DriveMixConfig& config);
float scaleBatteryVoltage(float adcVolts, float dividerMultiplier, float calibration);
BatterySafetyResult evaluateBatterySafety(float packVolts, bool enabled, float warnVoltage,
                                          float criticalVoltage, bool derateEnabled,
                                          float derateVoltage);
float applyAxisCalibration(float value, const AxisCalibration& calibration, bool positiveOnly);
bool shouldAutoInvertDrive(bool enabled, bool invertible, bool imuPresent, float accelZ,
                           float threshold);
bool timedActionActive(uint32_t nowMs, uint32_t untilMs, bool active);
bool cooldownReady(uint32_t nowMs, uint32_t lastStartMs, uint32_t cooldownMs);
uint32_t cooldownRemainingMs(uint32_t nowMs, uint32_t lastStartMs, uint32_t cooldownMs);
WeaponInputResult resolveWeaponInput(float analogValue, bool buttonPressed, bool toggleMode,
                                     bool toggleLatched, bool inputWasPressed,
                                     float buttonPower);
ArmBlockReason evaluateArmBlock(bool otaInProgress, bool benchMode, bool batteryCritical,
                                bool freshControlSource, bool serialSource,
                                bool liveOutputEnabled);
const char* armBlockReasonText(ArmBlockReason reason);

}  // namespace antcore
