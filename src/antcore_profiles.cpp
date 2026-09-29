#include "antcore_file_store.h"
#include "antcore_profiles.h"

#include <LittleFS.h>

#include <cctype>
#include <cstdio>

#include "antcore_config.h"

namespace antcore_profiles {

namespace {



uint8_t profileFileCountExcept(const String& pathToReplace) {
  File root = LittleFS.open(PROFILE_DIR);
  if (!root || !root.isDirectory()) return 0;
  uint8_t count = 0;
  for (File file = root.openNextFile(); file; file = root.openNextFile()) {
    if (file.isDirectory()) continue;
    String path = file.name();
    if (!path.endsWith(".json")) continue;
    if (path == pathToReplace) continue;
    count++;
  }
  return count;
}

}  // namespace

String normalizeProfileName(const char* rawName) {
  String name = rawName == nullptr ? "" : String(rawName);
  name.trim();
  String out;
  bool lastWasSpace = false;
  for (uint16_t i = 0; i < name.length() && out.length() < sizeof(AppConfig::activeProfile) - 1; i++) {
    char c = name.charAt(i);
    const bool safePrintable = c >= 32 && c <= 126 && c != '/' && c != '\\' && c != '"' && c != '\'';
    if (!safePrintable) continue;
    if (isspace(static_cast<unsigned char>(c))) {
      if (!lastWasSpace && out.length() > 0) {
        out += ' ';
        lastWasSpace = true;
      }
    } else {
      out += c;
      lastWasSpace = false;
    }
  }
  out.trim();
  return out;
}

uint32_t profileNameHash(const String& name) {
  uint32_t hash = 2166136261UL;
  for (uint16_t i = 0; i < name.length(); i++) {
    hash ^= static_cast<uint8_t>(name.charAt(i));
    hash *= 16777619UL;
  }
  return hash;
}

static String profileSlugWithLimit(const String& name, size_t prefixLimit) {
  String slug;
  bool lastWasDash = false;
  for (uint16_t i = 0; i < name.length() && slug.length() < prefixLimit; i++) {
    char c = name.charAt(i);
    if (isalnum(static_cast<unsigned char>(c))) {
      slug += static_cast<char>(tolower(static_cast<unsigned char>(c)));
      lastWasDash = false;
    } else if (!lastWasDash && slug.length() > 0) {
      slug += '-';
      lastWasDash = true;
    }
  }
  while (slug.endsWith("-")) {
    slug.remove(slug.length() - 1);
  }
  if (slug.length() == 0) slug = "profile";
  char suffix[10];
  snprintf(suffix, sizeof(suffix), "-%08lx", static_cast<unsigned long>(profileNameHash(name)));
  return slug + suffix;
}

String profileSlug(const String& name) {
  // PlatformIO's mklittlefs image records a 32-byte filename limit, even when
  // the firmware SDK supports longer names: 18 + '-12345678' + '.json' = 32.
  return profileSlugWithLimit(name, 18);
}

String profilePathForName(const String& name) {
  return String(PROFILE_DIR) + "/" + profileSlug(name) + ".json";
}

static String storedProfilePathForName(const String& name) {
  const String path = profilePathForName(name);
  if (LittleFS.exists(path)) return path;
  // Preserve access to files written by earlier firmware on a filesystem
  // formatted with a wider filename limit.
  const String legacyPath = String(PROFILE_DIR) + "/" + profileSlugWithLimit(name, 28) + ".json";
  return legacyPath != path && LittleFS.exists(legacyPath) ? legacyPath : path;
}

bool ensureProfileDir(bool fsMounted) {
  if (!fsMounted) return false;
  if (LittleFS.exists(PROFILE_DIR)) return true;
  return LittleFS.mkdir(PROFILE_DIR);
}

String profileDisplayNameFromFile(File& file) {
  StaticJsonDocument<64> filter;
  filter["activeProfile"] = true;
  DynamicJsonDocument doc(160);
  DeserializationError err = deserializeJson(doc, file, DeserializationOption::Filter(filter));
  if (!err && doc["activeProfile"].is<const char*>()) {
    String name = normalizeProfileName(doc["activeProfile"].as<const char*>());
    if (name.length() > 0) return name;
  }
  String fallback = file.name();
  int slash = fallback.lastIndexOf('/');
  if (slash >= 0) fallback = fallback.substring(slash + 1);
  if (fallback.endsWith(".json")) fallback.remove(fallback.length() - 5);
  return fallback;
}

void makeProfilesJson(bool fsMounted, const char* activeProfile, JsonDocument& doc) {
  doc.clear();
  doc["ok"] = fsMounted;
  doc["activeProfile"] = activeProfile == nullptr ? "" : activeProfile;
  JsonArray profiles = doc.createNestedArray("profiles");
  if (!ensureProfileDir(fsMounted)) return;

  File root = LittleFS.open(PROFILE_DIR);
  if (!root || !root.isDirectory()) return;
  uint8_t count = 0;
  for (File file = root.openNextFile(); file && count < MAX_PROFILE_COUNT; file = root.openNextFile()) {
    if (file.isDirectory()) continue;
    String path = file.name();
    if (!path.endsWith(".json")) continue;
    String name = profileDisplayNameFromFile(file);
    JsonObject profile = profiles.createNestedObject();
    profile["name"] = name;
    profile["path"] = path;
    profile["size"] = file.size();
    count++;
  }
}

bool writeNamedProfile(bool fsMounted, const AppConfig& config, const String& profileName,
                       JsonDocument& workspace, String& reason) {
  const String savedName = normalizeProfileName(profileName.c_str());
  if (savedName.length() == 0) {
    reason = "profile name required";
    return false;
  }
  if (!ensureProfileDir(fsMounted)) {
    reason = "profile storage unavailable";
    return false;
  }
  const String path = storedProfilePathForName(savedName);
  if (!LittleFS.exists(path) && profileFileCountExcept(path) >= MAX_PROFILE_COUNT) {
    reason = "profile list full";
    return false;
  }

  String out;
  {
    JsonDocument& doc = workspace;
    antcore_config::configToJson(config, doc);
    const size_t length = measureJson(doc);
    if (doc.capacity() == 0 || doc.overflowed() || !out.reserve(length) ||
        serializeJson(doc, out) != length || out.length() != length) {
      reason = "profile serialization failed";
      return false;
    }
  }
  char temporaryName[24];
  snprintf(temporaryName, sizeof(temporaryName), "/.profile-%08lx.tmp",
           static_cast<unsigned long>(profileNameHash(savedName)));
  const String temporaryPath = String(PROFILE_DIR) + temporaryName;
  if (!antcore_storage::writeTextAtomic(LittleFS, path.c_str(), out, temporaryPath.c_str())) {
    reason = "profile write failed";
    return false;
  }
  reason = "profile saved";
  return true;
}

bool readNamedProfile(bool fsMounted, const String& requestedName, JsonDocument& doc,
                      String& loadedName, String& reason) {
  loadedName = normalizeProfileName(requestedName.c_str());
  if (loadedName.length() == 0) {
    reason = "profile name required";
    return false;
  }
  if (!fsMounted) {
    reason = "profile storage unavailable";
    return false;
  }
  File file = LittleFS.open(storedProfilePathForName(loadedName), FILE_READ);
  if (!file) {
    reason = "profile not found";
    return false;
  }
  DeserializationError err = deserializeJson(doc, file);
  if (err) {
    reason = "profile JSON invalid";
    return false;
  }
  reason = "profile loaded";
  return true;
}

bool deleteNamedProfile(bool fsMounted, const String& requestedName, String& deletedName,
                        String& reason) {
  deletedName = normalizeProfileName(requestedName.c_str());
  if (deletedName.length() == 0) {
    reason = "profile name required";
    return false;
  }
  if (!fsMounted) {
    reason = "profile storage unavailable";
    return false;
  }
  String path = storedProfilePathForName(deletedName);
  if (!LittleFS.exists(path)) {
    reason = "profile not found";
    return false;
  }
  const bool ok = LittleFS.remove(path);
  reason = ok ? "profile deleted" : "profile delete failed";
  return ok;
}

}  // namespace antcore_profiles
