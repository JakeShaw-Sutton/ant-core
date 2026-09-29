#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace antcore_app {

// Setup/loop task only: clears the shared scratch document. Consume all JSON
// variants before another borrow or telemetry refresh. Never use from async
// HTTP/WebSocket callbacks; those tasks may only copy published snapshots.
JsonDocument& borrowLoopJsonDocument();

// Build on the loop task; HTTP/WebSocket callbacks only copy published JSON.
void initTelemetry();
void refreshTelemetry();
String cachedStatusJson(bool includeSensitive = false);
String cachedConfigJson();
String cachedValidationJson();

}  // namespace antcore_app
