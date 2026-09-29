#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include <AsyncJson.h>

namespace antcore_app {

void registerOutputTestsRoutes() {
  AsyncCallbackJsonWebHandler* liveOutputHandler =
      new AsyncCallbackJsonWebHandler("/api/test/live-output", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        RuntimeCommand command;
        command.type = RuntimeCommandType::LiveOutputSet;
        command.boolValue = json["enabled"] | false;
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 256);
  liveOutputHandler->setMethod(HTTP_POST);
  webState.server.addHandler(liveOutputHandler);

  AsyncCallbackJsonWebHandler* motorTestHandler =
      new AsyncCallbackJsonWebHandler("/api/test/motor", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        RuntimeCommand command;
        command.type = RuntimeCommandType::MotorTest;
        command.index = json["motor"] | 0;
        command.power = json["power"] | 0.0f;
        command.durationMs = json["durationMs"] | 250;
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 384);
  motorTestHandler->setMethod(HTTP_POST);
  webState.server.addHandler(motorTestHandler);

  AsyncCallbackJsonWebHandler* servoTestHandler =
      new AsyncCallbackJsonWebHandler("/api/test/servo", [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!requireAuth(request)) return;
        RuntimeCommand command;
        command.type = RuntimeCommandType::ServoTest;
        command.index = json["servo"] | 0;
        command.servoUs = json["us"] | 1500;
        command.durationMs = json["durationMs"] | 250;
        submitRuntimeCommand(command);
        sendRuntimeCommandJson(request, command);
      }, 384);
  servoTestHandler->setMethod(HTTP_POST);
  webState.server.addHandler(servoTestHandler);
}

}  // namespace antcore_app
