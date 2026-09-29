#pragma once

#include <Arduino.h>

#include "antcore_app_indicator.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

enum class StatusPattern {
  Booting,
  WifiConnecting,
  WifiConnected,
  ApFallback,
  SafeBoot,
  Armed,
  Ota,
};

struct IndicatorState {
  bool statusLedOn = false;
  bool statusLedAvailable = false;
  bool auxStatusLedOn = false;
  bool auxStatusLedAvailable = false;
  const char* statusIndicator = "booting";
};

extern IndicatorState indicatorState;

}  // namespace antcore_app
