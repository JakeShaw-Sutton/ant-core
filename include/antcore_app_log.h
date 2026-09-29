#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "antcore_firmware_config.h"

namespace antcore_app {

size_t blackboxLogSize();
void serviceLogQueue(uint8_t maxLines = 4);
void serviceBlackboxQueue(uint8_t maxLines = 1);
void addLogsToJson(JsonArray logs, uint8_t maxLines = LOG_RING_SIZE);
void clearLogRing();
void addLog(const char* level, const String& message, bool persist = true);

}  // namespace antcore_app
