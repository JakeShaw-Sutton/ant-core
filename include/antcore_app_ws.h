#pragma once

#include <Arduino.h>

namespace antcore_app {

void initWebSocket();
void broadcastStatus();
void broadcastWebSocketText(String text, bool telemetry = false);

}  // namespace antcore_app
