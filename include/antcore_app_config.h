#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace antcore_app {

void resetDefaultConfig();
void configToJson(JsonDocument& doc, bool includeSecrets = true);
void sanitizeConfig();
bool buildConfigValidation(JsonObject validation, String* firstError = nullptr);
bool configIsValid(String* firstError = nullptr);
void applyConfigJson(JsonVariantConst root);
bool saveConfig();
void loadConfig();
void applyHeadlessWifiDefaults();
bool ensureProfileDir();
void makeProfilesJson(JsonDocument& doc);
void makePacksJson(JsonDocument& doc);
bool saveNamedProfile(const String& requestedName, String& savedName, String& reason);
bool loadNamedProfile(const String& requestedName, String& loadedName, String& reason);
bool deleteNamedProfile(const String& requestedName, String& deletedName, String& reason);

}  // namespace antcore_app
