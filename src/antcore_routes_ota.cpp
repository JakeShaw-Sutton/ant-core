#include "app/config_state.h"
#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include <Update.h>

namespace antcore_app {

void registerOtaRoutes() {
  webState.server.on(
      "/api/ota/firmware", HTTP_POST,
      [](AsyncWebServerRequest* request) {
        if (!antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) {
          antcore_auth::sendAuthFailure(request);
          return;
        }
        const bool ok = !Update.hasError();
        RuntimeCommand command;
        command.type = RuntimeCommandType::OtaFinish;
        command.boolValue = ok;
        submitRuntimeCommand(command);
        DynamicJsonDocument doc(160);
        doc["ok"] = command.ok;
        doc["message"] = ok ? "firmware uploaded, rebooting" : "firmware upload failed";
        sendJson(request, doc, command.status);
      },
      [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {
          if (!antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) {
            Update.abort();
            return;
          }
          RuntimeCommand command;
          command.type = RuntimeCommandType::OtaBeginFirmware;
          command.text = filename;
          submitRuntimeCommand(command);
          if (!command.ok) {
            Update.abort();
            return;
          }
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError() && Update.write(data, len) != len) {
          Update.printError(Serial);
        }
        if (final) {
          if (!Update.end(true)) {
            Update.printError(Serial);
          } else {
            addLog("INFO", "firmware OTA complete");
          }
        }
      });

  webState.server.on(
      "/api/ota/filesystem", HTTP_POST,
      [](AsyncWebServerRequest* request) {
        if (!antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) {
          antcore_auth::sendAuthFailure(request);
          return;
        }
        const bool ok = !Update.hasError();
        RuntimeCommand command;
        command.type = RuntimeCommandType::OtaFinish;
        command.boolValue = ok;
        submitRuntimeCommand(command);
        DynamicJsonDocument doc(160);
        doc["ok"] = command.ok;
        doc["message"] = ok ? "filesystem uploaded, rebooting" : "filesystem upload failed";
        sendJson(request, doc, command.status);
      },
      [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {
          if (!antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) {
            Update.abort();
            return;
          }
          RuntimeCommand command;
          command.type = RuntimeCommandType::OtaBeginFilesystem;
          command.text = filename;
          submitRuntimeCommand(command);
          if (!command.ok) {
            Update.abort();
            return;
          }
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError() && Update.write(data, len) != len) {
          Update.printError(Serial);
        }
        if (final) {
          if (!Update.end(true)) {
            Update.printError(Serial);
          } else {
            addLog("INFO", "filesystem OTA complete");
          }
        }
      });
}

}  // namespace antcore_app
