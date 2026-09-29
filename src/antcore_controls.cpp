#include "antcore_controls.h"

#include <cstring>

using antcore::clampFloat;

bool stringInList(const char* value, const char* const* list, size_t count) {
  if (value == nullptr || strlen(value) == 0) return false;
  for (size_t i = 0; i < count; i++) {
    if (!strcmp(value, list[i])) return true;
  }
  return false;
}

bool isKnownAxisName(const char* value) {
  return stringInList(value, AXIS_NAMES, sizeof(AXIS_NAMES) / sizeof(AXIS_NAMES[0]));
}

bool isKnownButtonName(const char* value) {
  return stringInList(value, BUTTON_NAMES, sizeof(BUTTON_NAMES) / sizeof(BUTTON_NAMES[0]));
}

bool isKnownInputName(const char* value) {
  return isKnownAxisName(value) || isKnownButtonName(value);
}

bool isKnownActionName(const char* value) {
  return stringInList(value, ACTION_NAMES, sizeof(ACTION_NAMES) / sizeof(ACTION_NAMES[0]));
}

bool readButtonByName(const ControlState& state, const char* name) {
  if (name == nullptr) return false;
  if (!strcmp(name, "a")) return state.a;
  if (!strcmp(name, "b")) return state.b;
  if (!strcmp(name, "x")) return state.x;
  if (!strcmp(name, "y")) return state.y;
  if (!strcmp(name, "leftBumper")) return state.leftBumper;
  if (!strcmp(name, "rightBumper")) return state.rightBumper;
  if (!strcmp(name, "leftStickButton")) return state.leftStickButton;
  if (!strcmp(name, "rightStickButton")) return state.rightStickButton;
  if (!strcmp(name, "dpadUp")) return state.dpadUp;
  if (!strcmp(name, "dpadDown")) return state.dpadDown;
  if (!strcmp(name, "dpadLeft")) return state.dpadLeft;
  if (!strcmp(name, "dpadRight")) return state.dpadRight;
  if (!strcmp(name, "share")) return state.share;
  if (!strcmp(name, "menu")) return state.menu;
  if (!strcmp(name, "view")) return state.view;
  if (!strcmp(name, "xbox")) return state.xbox;
  return false;
}

float readAnalogInputByName(const ControlState& state, const char* name) {
  if (name == nullptr) return 0.0f;
  if (!strcmp(name, "leftX")) return state.leftX;
  if (!strcmp(name, "leftY")) return state.leftY;
  if (!strcmp(name, "rightX")) return state.rightX;
  if (!strcmp(name, "rightY")) return state.rightY;
  if (!strcmp(name, "leftTrigger")) return state.leftTrigger;
  if (!strcmp(name, "rightTrigger")) return state.rightTrigger;
  return readButtonByName(state, name) ? 1.0f : 0.0f;
}

int8_t axisIndexByName(const char* name) {
  if (name == nullptr) return -1;
  for (uint8_t i = 0; i < AXIS_COUNT; i++) {
    if (!strcmp(name, AXIS_NAMES[i])) return i;
  }
  return -1;
}

bool axisIsPositiveOnly(uint8_t index) {
  return index >= 4;
}

AxisCalibration defaultAxisCalibration(uint8_t index) {
  AxisCalibration calibration;
  if (axisIsPositiveOnly(index)) {
    calibration.minValue = 0.0f;
    calibration.centerValue = 0.0f;
    calibration.maxValue = 1.0f;
  }
  return calibration;
}

void sanitizeAxisCalibration(AxisCalibration& calibration, uint8_t index) {
  const bool positiveOnly = axisIsPositiveOnly(index);
  calibration.minValue = clampFloat(calibration.minValue, positiveOnly ? 0.0f : -1.0f, 1.0f);
  calibration.centerValue = clampFloat(calibration.centerValue, positiveOnly ? 0.0f : -1.0f, 1.0f);
  calibration.maxValue = clampFloat(calibration.maxValue, positiveOnly ? 0.05f : -1.0f, 1.0f);
  calibration.deadband = clampFloat(calibration.deadband, 0.0f, 0.45f);
  const bool invalidRange = positiveOnly
                                ? (calibration.maxValue - calibration.minValue < 0.05f ||
                                   calibration.centerValue < calibration.minValue ||
                                   calibration.centerValue >= calibration.maxValue)
                                : (calibration.maxValue - calibration.minValue < 0.05f ||
                                   calibration.centerValue <= calibration.minValue ||
                                   calibration.centerValue >= calibration.maxValue);
  if (invalidRange) {
    calibration = defaultAxisCalibration(index);
  }
}

float calibratedAxisValue(uint8_t index, float value,
                          const ControlConfig::CalibrationConfig& calibrationConfig) {
  if (!calibrationConfig.enabled || index >= AXIS_COUNT) return value;
  return antcore::applyAxisCalibration(value, calibrationConfig.axes[index], axisIsPositiveOnly(index));
}

ControlState calibratedControlState(const ControlState& raw,
                                    const ControlConfig::CalibrationConfig& calibrationConfig) {
  ControlState out = raw;
  out.leftX = calibratedAxisValue(0, raw.leftX, calibrationConfig);
  out.leftY = calibratedAxisValue(1, raw.leftY, calibrationConfig);
  out.rightX = calibratedAxisValue(2, raw.rightX, calibrationConfig);
  out.rightY = calibratedAxisValue(3, raw.rightY, calibrationConfig);
  out.leftTrigger = calibratedAxisValue(4, raw.leftTrigger, calibrationConfig);
  out.rightTrigger = calibratedAxisValue(5, raw.rightTrigger, calibrationConfig);
  return out;
}
