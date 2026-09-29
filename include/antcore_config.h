#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "antcore_firmware_config.h"

namespace antcore_config {

bool isDefaultAdminPin(const AppConfig& config);
void resetDefaultConfig(AppConfig& config);
void configToJson(const AppConfig& config, JsonDocument& doc, bool includeSecrets = true);
bool sanitizeConfig(AppConfig& config);
bool buildConfigValidation(const AppConfig& config, bool imuPresent, JsonObject validation,
                           String* firstError = nullptr);
bool configIsValid(const AppConfig& config, bool imuPresent, String* firstError = nullptr);
bool applyConfigJson(AppConfig& config, JsonVariantConst root);

}  // namespace antcore_config
