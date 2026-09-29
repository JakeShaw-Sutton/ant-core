#pragma once

#include <cstddef>
#include <cstdint>

#include "antcore_firmware_config.h"

bool stringInList(const char* value, const char* const* list, size_t count);
bool isKnownAxisName(const char* value);
bool isKnownButtonName(const char* value);
bool isKnownInputName(const char* value);
bool isKnownActionName(const char* value);
bool readButtonByName(const ControlState& state, const char* name);
float readAnalogInputByName(const ControlState& state, const char* name);
int8_t axisIndexByName(const char* name);
bool axisIsPositiveOnly(uint8_t index);
AxisCalibration defaultAxisCalibration(uint8_t index);
void sanitizeAxisCalibration(AxisCalibration& calibration, uint8_t index);
float calibratedAxisValue(uint8_t index, float value,
                          const ControlConfig::CalibrationConfig& calibrationConfig);
ControlState calibratedControlState(const ControlState& raw,
                                    const ControlConfig::CalibrationConfig& calibrationConfig);
