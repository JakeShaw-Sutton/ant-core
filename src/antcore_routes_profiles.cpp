#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include "antcore_app_config.h"
#include "antcore_packs.h"
#include <AsyncJson.h>

namespace antcore_app {

void registerProfilesRoutes() {
  webState.server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(PROFILE_LIST_JSON_CAPACITY);
    makeProfilesJson(doc);
    sendJson(request, doc, configState.fsMounted ? 200 : 503);
  });

  webState.server.on("/api/packs", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(PACK_LIST_JSON_CAPACITY);
    makePacksJson(doc);
    sendJson(request, doc, configState.fsMounted ? 200 : 503);
  });

  AsyncCallbackJsonWebHandler* packSaveHandler =
      new AsyncCallbackJsonWebHandler("/api/packs/save", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        if (commandsState.filesystemOtaInProgress) {
          DynamicJsonDocument doc(160);
          doc["ok"] = false;
          doc["message"] = "filesystem OTA in progress";
          sendJson(request, doc, 503);
          return;
        }
        String id;
        String reason;
        const bool ok = antcore_packs::savePack(configState.fsMounted, json.as<JsonVariantConst>(), id, reason);
        DynamicJsonDocument doc(256);
        doc["ok"] = ok;
        doc["id"] = id;
        doc["message"] = reason;
        sendJson(request, doc, ok ? 200 : 400);
        addLog(ok ? "INFO" : "ERROR", ok ? "battery pack saved: " + id : "battery pack save failed: " + reason);
      }, 768);
  packSaveHandler->setMethod(HTTP_POST);
  webState.server.addHandler(packSaveHandler);

  AsyncCallbackJsonWebHandler* packDeleteHandler =
      new AsyncCallbackJsonWebHandler("/api/packs/delete", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        if (commandsState.filesystemOtaInProgress) {
          DynamicJsonDocument doc(160);
          doc["ok"] = false;
          doc["message"] = "filesystem OTA in progress";
          sendJson(request, doc, 503);
          return;
        }
        String id;
        String reason;
        const bool ok = antcore_packs::deletePack(configState.fsMounted, String(json["id"] | ""), id, reason);
        DynamicJsonDocument doc(192);
        doc["ok"] = ok;
        doc["id"] = id;
        doc["message"] = reason;
        sendJson(request, doc, ok ? 200 : 404);
        addLog(ok ? "INFO" : "ERROR", ok ? "battery pack deleted: " + id : "battery pack delete failed: " + reason);
      }, 384);
  packDeleteHandler->setMethod(HTTP_POST);
  webState.server.addHandler(packDeleteHandler);

  AsyncCallbackJsonWebHandler* profileSaveHandler =
      new AsyncCallbackJsonWebHandler("/api/profiles/save", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        RuntimeCommand command;
        command.type = RuntimeCommandType::SaveProfile;
        command.text = String(json["name"] | configState.cfg.activeProfile);
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 512);
  profileSaveHandler->setMethod(HTTP_POST);
  webState.server.addHandler(profileSaveHandler);

  AsyncCallbackJsonWebHandler* profileLoadHandler =
      new AsyncCallbackJsonWebHandler("/api/profiles/load", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        RuntimeCommand command;
        command.type = RuntimeCommandType::LoadProfile;
        command.text = String(json["name"] | "");
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 512);
  profileLoadHandler->setMethod(HTTP_POST);
  webState.server.addHandler(profileLoadHandler);

  AsyncCallbackJsonWebHandler* profileDeleteHandler =
      new AsyncCallbackJsonWebHandler("/api/profiles/delete", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        if (commandsState.filesystemOtaInProgress) {
          DynamicJsonDocument doc(160);
          doc["ok"] = false;
          doc["message"] = "filesystem OTA in progress";
          sendJson(request, doc, 503);
          return;
        }
        String name;
        String reason;
        const bool ok = deleteNamedProfile(String(json["name"] | ""), name, reason);
        DynamicJsonDocument doc(256);
        doc["ok"] = ok;
        doc["name"] = name;
        doc["message"] = reason;
        sendJson(request, doc, ok ? 200 : 404);
        addLog(ok ? "INFO" : "ERROR", ok ? "profile deleted: " + name : "profile delete failed: " + reason);
      }, 512);
  profileDeleteHandler->setMethod(HTTP_POST);
  webState.server.addHandler(profileDeleteHandler);
}

}  // namespace antcore_app
