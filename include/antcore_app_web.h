#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "antcore_firmware_config.h"
#include <ESPAsyncWebServer.h>
#include "antcore_auth.h"
#include "antcore_app_commands.h"

namespace antcore_app {

bool isDefaultAdminPin();
bool requireAuth(AsyncWebServerRequest* request);
void sendCachedJson(AsyncWebServerRequest* request, String json);
void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code = 200);
void sendRuntimeCommandJson(AsyncWebServerRequest* request, RuntimeCommand& command);
String cameraUrlForIp(IPAddress ip, const char* path);
void registerApiRoutes();

}  // namespace antcore_app
