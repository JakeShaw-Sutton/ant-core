#pragma once

#include "antcore_app_web.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct WebState {
  AsyncWebServer server{80};
  bool webServerReady = false;
  AuthState authState;
};

extern WebState webState;

}  // namespace antcore_app
