#pragma once

#include <Preferences.h>
#include "antcore_firmware_config.h"

#include "antcore_app_config.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct ConfigState {
  AppConfig cfg;
  Preferences prefs;
  bool fsMounted = false;
  bool headlessStaDefaultsApplied = false;
};

extern ConfigState configState;

}  // namespace antcore_app
