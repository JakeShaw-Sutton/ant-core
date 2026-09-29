#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "antcore_firmware_config.h"

namespace antcore_profiles {

String normalizeProfileName(const char* rawName);
uint32_t profileNameHash(const String& name);
String profileSlug(const String& name);
String profilePathForName(const String& name);
bool ensureProfileDir(bool fsMounted);
String profileDisplayNameFromFile(File& file);
void makeProfilesJson(bool fsMounted, const char* activeProfile, JsonDocument& doc);
bool writeNamedProfile(bool fsMounted, const AppConfig& config, const String& profileName,
                       JsonDocument& workspace, String& reason);
bool readNamedProfile(bool fsMounted, const String& requestedName, JsonDocument& doc,
                      String& loadedName, String& reason);
bool deleteNamedProfile(bool fsMounted, const String& requestedName, String& deletedName,
                        String& reason);

}  // namespace antcore_profiles
