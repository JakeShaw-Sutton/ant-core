#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include <AsyncJson.h>

namespace antcore_app {

void registerControlRoutes() {
  webState.server.on("/api/arm", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::Arm;
    command.source = "web";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });

  webState.server.on("/api/disarm", HTTP_POST, [](AsyncWebServerRequest* request) {
    RuntimeCommand command;
    command.type = RuntimeCommandType::Disarm;
    command.source = "web";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });

  AsyncCallbackJsonWebHandler* pitHandler =
      new AsyncCallbackJsonWebHandler("/api/pit", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        const bool enabled = json["enabled"] | !configState.cfg.safety.pitMode;
        RuntimeCommand command;
        command.type = RuntimeCommandType::PitSet;
        command.boolValue = enabled;
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 256);
  pitHandler->setMethod(HTTP_POST);
  webState.server.addHandler(pitHandler);

  webState.server.on("/api/weapon/arm", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::WeaponArm;
    command.source = "web";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });

  webState.server.on("/api/weapon/disarm", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::WeaponDisarm;
    command.source = "web";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });

  webState.server.on("/api/control/release", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::ReleaseWebControl;
    command.source = "web";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });

  webState.server.on("/api/ble/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    if (!inputsState.bleReady) {
      DynamicJsonDocument doc(160);
      doc["ok"] = false;
      doc["message"] = lifecycleState.optionalPeripheralsSkipped ? lifecycleState.optionalPeripheralSkipReason : "BLE not ready yet";
      sendJson(request, doc, 503);
      return;
    }
    BLEGamepadClient::getAutoScan()->enable();
    BLEGamepadClient::getAutoScan()->notify();
    DynamicJsonDocument doc(128);
    doc["ok"] = true;
    doc["scanEnabled"] = true;
    sendJson(request, doc);
    addLog("INFO", "BLE scan requested");
  });

  webState.server.on("/api/ble/forget", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::BleForget;
    command.source = "BLE forget";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });
}

}  // namespace antcore_app
