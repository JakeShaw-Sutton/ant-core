#pragma once

#include <Arduino.h>
#include "antcore_control_owner.h"

namespace antcore_app {

antcore::WebControlSnapshot webControlSnapshot();
void getWebControlSnapshot(ControlState& state, char* driverId, size_t driverIdLen, uint32_t& driverLastMs);
void clearWebControlLock();
bool claimWebDriverId(uint32_t connectionId, const char* id, uint32_t now);
bool releaseWebDriverId(uint32_t connectionId);
bool publishWebControlFrame(uint32_t connectionId, const char* id, const ControlState& state, uint32_t now);
bool xboxControlFresh(uint32_t now);
bool hasFreshControlSource();
void updateXboxStateFromController(uint32_t now);
void initBle();

}  // namespace antcore_app
