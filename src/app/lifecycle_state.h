#pragma once

#include <Arduino.h>
#include "antcore_firmware_config.h"
#include <esp_system.h>

#include "antcore_app_lifecycle.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct LifecycleState {
  uint32_t lastSensorMs = 0;
  uint32_t lastStatusBroadcastMs = 0;
  bool serialConnectedAtBoot = false;
  String bootResetReason = "unknown";
  bool optionalPeripheralsSkipped = false;
  bool bootMarkedStable = false;
  uint32_t optionalPeripheralStartMs = 0;
  String optionalPeripheralSkipReason = "";
};

extern LifecycleState lifecycleState;

extern uint32_t rtcBootGuardMagic;

extern uint8_t rtcBootCrashCount;

constexpr uint32_t BOOT_GUARD_MAGIC = 0xA17C0E01;

constexpr uint32_t BOOT_STABLE_MS = 12000;

constexpr uint32_t OPTIONAL_PERIPHERAL_DELAY_MS = 2500;

constexpr uint32_t CAMERA_INIT_EXTRA_DELAY_MS = 2000;

}  // namespace antcore_app
