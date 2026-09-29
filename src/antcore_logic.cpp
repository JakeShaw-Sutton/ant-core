#include "antcore_logic.h"

#include <cmath>

namespace antcore {

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

float compensateMotor(float target, float trim, bool invert, float maxOutput) {
  if (!std::isfinite(target)) return 0.0f;
  target *= 1.0f + clampFloat(trim, -0.25f, 0.25f);
  if (invert) target = -target;
  return clampFloat(target, -maxOutput, maxOutput);
}

float applyDeadbandExpo(float value, float deadband, float expo) {
  value = clampFloat(value, -1.0f, 1.0f);
  deadband = clampFloat(deadband, 0.0f, 0.95f);
  expo = clampFloat(expo, 0.0f, 1.0f);
  const float mag = std::fabs(value);
  if (mag <= deadband) return 0.0f;
  const float normalized = clampFloat((mag - deadband) / (1.0f - deadband), 0.0f, 1.0f);
  const float curved = (expo * normalized * normalized * normalized) + ((1.0f - expo) * normalized);
  return value < 0.0f ? -curved : curved;
}

DriveMixResult computeDriveMix(const DriveInputs& inputs, const DriveMixConfig& config) {
  float scale = 1.0f;
  if (inputs.precision) {
    scale = clampFloat(config.precisionScale, 0.1f, 1.0f);
  } else if (inputs.turbo) {
    scale = clampFloat(config.turboScale, 0.1f, 1.0f);
  }
  if (inputs.derate) {
    scale = scale < config.derateScale ? scale : clampFloat(config.derateScale, 0.15f, 1.0f);
  }

  float left = 0.0f;
  float right = 0.0f;
  if (config.tankMode) {
    left = applyDeadbandExpo(inputs.leftY, config.deadband, config.expo) * config.throttleScale;
    right = applyDeadbandExpo(inputs.rightY, config.deadband, config.expo) * config.throttleScale;
    if (config.driveInverted) {
      // Reverse translation while retaining steering handedness, as in arcade.
      const float originalLeft = left;
      left = -right;
      right = -originalLeft;
    }
  } else {
    float throttle = applyDeadbandExpo(inputs.leftY, config.deadband, config.expo) * config.throttleScale;
    const float turn = clampFloat(applyDeadbandExpo(inputs.leftX, config.deadband, config.expo) + inputs.turnCorrection,
                                  -1.0f, 1.0f) *
                       config.turnScale;
    if (config.driveInverted) throttle = -throttle;
    left = throttle + turn;
    right = throttle - turn;
  }

  left *= scale;
  right *= scale;
  const float maxMag = std::fmax(1.0f, std::fmax(std::fabs(left), std::fabs(right)));
  DriveMixResult result;
  result.left = left / maxMag;
  result.right = right / maxMag;
  result.scale = scale;
  return result;
}

float scaleBatteryVoltage(float adcVolts, float dividerMultiplier, float calibration) {
  adcVolts = adcVolts < 0.0f ? 0.0f : adcVolts;
  dividerMultiplier = clampFloat(dividerMultiplier, 1.0f, 10.0f);
  calibration = clampFloat(calibration, 0.1f, 5.0f);
  return adcVolts * dividerMultiplier * calibration;
}

BatterySafetyResult evaluateBatterySafety(float packVolts, bool enabled, float warnVoltage,
                                          float criticalVoltage, bool derateEnabled,
                                          float derateVoltage) {
  BatterySafetyResult result;
  result.cellVolts = packVolts / 2.0f;
  if (!enabled) return result;
  result.warn = packVolts > 0.05f && packVolts <= warnVoltage;
  result.critical = packVolts <= criticalVoltage;
  result.derating = derateEnabled && packVolts > 0.05f && packVolts <= derateVoltage && !result.critical;
  return result;
}

float applyAxisCalibration(float value, const AxisCalibration& calibration, bool positiveOnly) {
  float minValue = calibration.minValue;
  float centerValue = calibration.centerValue;
  float maxValue = calibration.maxValue;
  const bool invalidRange = positiveOnly ? (centerValue < minValue || centerValue >= maxValue)
                                         : (centerValue <= minValue || centerValue >= maxValue);
  if (maxValue - minValue < 0.05f || invalidRange) {
    minValue = positiveOnly ? 0.0f : -1.0f;
    centerValue = 0.0f;
    maxValue = 1.0f;
  }

  float out = 0.0f;
  if (value >= centerValue) {
    out = (value - centerValue) / (maxValue - centerValue);
  } else {
    out = (value - centerValue) / (centerValue - minValue);
  }
  out = clampFloat(out, -1.0f, 1.0f);

  const float deadband = clampFloat(calibration.deadband, 0.0f, 0.45f);
  if (std::fabs(out) <= deadband) {
    out = 0.0f;
  } else {
    const float sign = out < 0.0f ? -1.0f : 1.0f;
    out = sign * clampFloat((std::fabs(out) - deadband) / (1.0f - deadband), 0.0f, 1.0f);
  }

  if (calibration.invert) out = -out;
  if (positiveOnly) out = clampFloat(out, 0.0f, 1.0f);
  return out;
}

bool shouldAutoInvertDrive(bool enabled, bool invertible, bool imuPresent, float accelZ,
                           float threshold) {
  return enabled && invertible && imuPresent && accelZ <= threshold;
}

bool timedActionActive(uint32_t nowMs, uint32_t untilMs, bool active) {
  return active && static_cast<int32_t>(nowMs - untilMs) < 0;
}

bool cooldownReady(uint32_t nowMs, uint32_t lastStartMs, uint32_t cooldownMs) {
  return lastStartMs == 0 || nowMs - lastStartMs >= cooldownMs;
}

uint32_t cooldownRemainingMs(uint32_t nowMs, uint32_t lastStartMs, uint32_t cooldownMs) {
  if (cooldownReady(nowMs, lastStartMs, cooldownMs)) return 0;
  return cooldownMs - (nowMs - lastStartMs);
}

WeaponInputResult resolveWeaponInput(float analogValue, bool buttonPressed, bool toggleMode,
                                     bool toggleLatched, bool inputWasPressed,
                                     float buttonPower) {
  analogValue = clampFloat(analogValue, -1.0f, 1.0f);
  buttonPower = clampFloat(buttonPower, -1.0f, 1.0f);
  const bool pressed = buttonPressed || analogValue > 0.5f;

  WeaponInputResult result;
  result.toggleLatched = toggleLatched;
  result.inputWasPressed = pressed;
  if (toggleMode) {
    if (pressed && !inputWasPressed) {
      result.toggleLatched = !result.toggleLatched;
    }
    result.value = result.toggleLatched ? buttonPower : 0.0f;
  } else {
    result.value = buttonPressed ? buttonPower : analogValue;
  }
  return result;
}

ArmBlockReason evaluateArmBlock(bool otaInProgress, bool benchMode, bool batteryCritical,
                                bool freshControlSource, bool serialSource,
                                bool liveOutputEnabled) {
  if (otaInProgress) return ArmBlockReason::OtaActive;
  if (benchMode) return ArmBlockReason::BenchMode;
  if (batteryCritical) return ArmBlockReason::BatteryCritical;
  (void)serialSource;
  if (!liveOutputEnabled && !freshControlSource) return ArmBlockReason::NoControlSource;
  return ArmBlockReason::None;
}

const char* armBlockReasonText(ArmBlockReason reason) {
  switch (reason) {
    case ArmBlockReason::OtaActive:
      return "OTA is active";
    case ArmBlockReason::BenchMode:
      return "bench mode active";
    case ArmBlockReason::BatteryCritical:
      return "battery critical";
    case ArmBlockReason::NoControlSource:
      return "no active control source";
    case ArmBlockReason::None:
    default:
      return "";
  }
}

}  // namespace antcore
