#pragma once

#include <Arduino.h>

#include "antcore_app_console.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct ConsoleState {
  String serialLine = "";
};

extern ConsoleState consoleState;

}  // namespace antcore_app
