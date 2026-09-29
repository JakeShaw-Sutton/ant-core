#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/lifecycle_state.h"
#include "app/peripherals_state.h"
#include "app/robot_state.h"
#include "antcore_app_config.h"
#include "antcore_app_commands.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_log.h"
#include "antcore_app_network.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_robot.h"
#include "antcore_app_telemetry.h"
#include <LittleFS.h>
#include "antcore_config.h"
#include "antcore_profiles.h"
#include "antcore_packs.h"
#include "antcore_file_store.h"
#include "antcore_output_test.h"
#include "camera_stream.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

ConfigState configState;

static void copyString(char* dest, size_t len, const char* value);
static bool writeTextAtomic(const char* path, const String& body);

static void copyString(char* dest, size_t len, const char* value) {
  if (len == 0) return;
  if (value == nullptr) value = "";
  strlcpy(dest, value, len);
}

void resetDefaultConfig() {
  antcore_config::resetDefaultConfig(configState.cfg);
}

void configToJson(JsonDocument& doc, bool includeSecrets) {
  antcore_config::configToJson(configState.cfg, doc, includeSecrets);
}

void sanitizeConfig() {
  if (antcore_config::sanitizeConfig(configState.cfg)) {
    antcore_output_test::disableLiveOutput(robotState.outputTest);
  }
}

bool buildConfigValidation(JsonObject validation, String* firstError) {
  return antcore_config::buildConfigValidation(configState.cfg, peripheralsState.imuTelemetry.present, validation, firstError);
}

bool configIsValid(String* firstError) {
  return antcore_config::configIsValid(configState.cfg, peripheralsState.imuTelemetry.present, firstError);
}

void applyConfigJson(JsonVariantConst root) {
  if (antcore_config::applyConfigJson(configState.cfg, root)) {
    antcore_output_test::disableLiveOutput(robotState.outputTest);
  }
}

static bool writeTextAtomic(const char* path, const String& body) {
  return configState.fsMounted && !commandsState.filesystemOtaInProgress && antcore_storage::writeTextAtomic(LittleFS, path, body);
}

bool saveConfig() {
  String out;
  {
    JsonDocument& doc = borrowLoopJsonDocument();
    configToJson(doc);
    if (doc.capacity() == 0 || doc.overflowed()) return false;
    const size_t length = measureJson(doc);
    // A partial String would otherwise pass the file writer's length check
    // and replace the saved configuration with truncated JSON.
    if (!out.reserve(length) || serializeJson(doc, out) != length || out.length() != length) return false;
  }
  if (configState.fsMounted) {
    const bool ok = writeTextAtomic(CONFIG_FILE_PATH, out);
    if (ok) configState.prefs.remove(PREF_CONFIG_KEY);
    return ok;
  }
  return configState.prefs.putString(PREF_CONFIG_KEY, out) == out.length();
}

void loadConfig() {
  resetDefaultConfig();
  String stored;
  const char* source = "";
  if (configState.fsMounted && LittleFS.exists(CONFIG_FILE_PATH)) {
    File file = LittleFS.open(CONFIG_FILE_PATH, FILE_READ);
    if (file) {
      stored = file.readString();
      file.close();
      source = "file";
    }
  }
  if (stored.length() == 0) {
    stored = configState.prefs.getString(PREF_CONFIG_KEY, "");
    if (stored.length() > 0) source = "preferences";
  }
  if (stored.length() > 0) {
    JsonDocument& doc = borrowLoopJsonDocument();
    DeserializationError err = deserializeJson(doc, stored);
    if (!err) {
      applyConfigJson(doc.as<JsonVariantConst>());
      // Applying copies values into AppConfig; migration may reuse the scratch.
      addLog("INFO", String("loaded saved config from ") + source);
      if (strcmp(source, "preferences") == 0 && configState.fsMounted && saveConfig()) {
        addLog("INFO", "migrated saved config to LittleFS");
      }
      return;
    }
    addLog("WARN", "saved config invalid, using defaults");
  } else {
    addLog("INFO", "using default config");
  }
  sanitizeConfig();
}

void applyHeadlessWifiDefaults() {
  configState.headlessStaDefaultsApplied = false;
  if (lifecycleState.serialConnectedAtBoot) return;
  if (!HEADLESS_STA_DEFAULT || DEFAULT_STA_SSID[0] == '\0') return;

  bool changed = false;
  if (!configState.cfg.wifi.staEnabled) {
    configState.cfg.wifi.staEnabled = true;
    changed = true;
  }
  if (strlen(configState.cfg.wifi.staSsid) == 0) {
    copyString(configState.cfg.wifi.staSsid, sizeof(configState.cfg.wifi.staSsid), DEFAULT_STA_SSID);
    changed = true;
  }
  if (strlen(configState.cfg.wifi.staPassword) == 0 && DEFAULT_STA_PASSWORD[0] != '\0') {
    copyString(configState.cfg.wifi.staPassword, sizeof(configState.cfg.wifi.staPassword), DEFAULT_STA_PASSWORD);
    changed = true;
  }

  if (changed) {
    sanitizeConfig();
    configState.headlessStaDefaultsApplied = true;
    addLog("INFO", "headless boot applied default STA " + String(configState.cfg.wifi.staSsid), false);
  }
}

bool ensureProfileDir() {
  return antcore_profiles::ensureProfileDir(configState.fsMounted);
}

void makeProfilesJson(JsonDocument& doc) {
  antcore_profiles::makeProfilesJson(configState.fsMounted, configState.cfg.activeProfile, doc);
}

void makePacksJson(JsonDocument& doc) {
  antcore_packs::makePacksJson(configState.fsMounted, doc);
}

bool saveNamedProfile(const String& requestedName, String& savedName, String& reason) {
  if (commandsState.filesystemOtaInProgress) {
    reason = "filesystem OTA in progress";
    return false;
  }
  savedName = antcore_profiles::normalizeProfileName(requestedName.c_str());
  if (savedName.length() == 0) {
    reason = "profile name required";
    return false;
  }
  if (!ensureProfileDir()) {
    reason = "profile storage unavailable";
    return false;
  }
  copyString(configState.cfg.activeProfile, sizeof(configState.cfg.activeProfile), savedName.c_str());
  sanitizeConfig();
  if (!saveConfig()) {
    reason = "profile config save failed";
    return false;
  }

  JsonDocument& workspace = borrowLoopJsonDocument();
  return antcore_profiles::writeNamedProfile(configState.fsMounted, configState.cfg, savedName, workspace, reason);
}

bool loadNamedProfile(const String& requestedName, String& loadedName, String& reason) {
  if (commandsState.filesystemOtaInProgress) {
    reason = "filesystem OTA in progress";
    return false;
  }
  {
    JsonDocument& doc = borrowLoopJsonDocument();
    if (!antcore_profiles::readNamedProfile(configState.fsMounted, requestedName, doc, loadedName, reason)) {
      return false;
    }
    disarmRobot("profile load");
    applyConfigJson(doc.as<JsonVariantConst>());
  }
  copyString(configState.cfg.activeProfile, sizeof(configState.cfg.activeProfile), loadedName.c_str());
  sanitizeConfig();
  const bool ok = saveConfig();
  if (ok && peripheralsState.cameraReady) applyCameraSensorSettings(configState.cfg.camera);
  if (ok) scheduleStaRestart();
  reason = ok ? "profile loaded" : "profile load save failed";
  return ok;
}

bool deleteNamedProfile(const String& requestedName, String& deletedName, String& reason) {
  if (commandsState.filesystemOtaInProgress) {
    reason = "filesystem OTA in progress";
    return false;
  }
  return antcore_profiles::deleteNamedProfile(configState.fsMounted, requestedName, deletedName, reason);
}

}  // namespace antcore_app
