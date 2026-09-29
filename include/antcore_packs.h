#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace antcore_packs {

String normalizePackId(const char* rawId, const char* rawName);
void makePacksJson(bool fsMounted, JsonDocument& doc);
bool savePack(bool fsMounted, JsonVariantConst root, String& savedId, String& reason);
bool deletePack(bool fsMounted, const String& requestedId, String& deletedId, String& reason);

}  // namespace antcore_packs
