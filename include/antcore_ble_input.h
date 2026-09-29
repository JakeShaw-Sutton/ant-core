#pragma once

#include <cstdint>

#include <BLEGamepadClient.h>

#include "antcore_firmware_config.h"

ControlState controlStateFromXboxControls(const XboxControlsState& value, uint32_t nowMs);
