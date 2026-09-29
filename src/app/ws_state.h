#pragma once

#include <ESPAsyncWebServer.h>

#include "antcore_app_ws.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct WsState {
  AsyncWebSocket ws{"/ws"};
  uint8_t wsClientCount = 0;
};

extern WsState wsState;

}  // namespace antcore_app
