#include "app/config_state.h"
#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include <AsyncJson.h>

namespace antcore_app {

void registerAuthRoutes() {
  AsyncCallbackJsonWebHandler* loginHandler =
      new AsyncCallbackJsonWebHandler("/api/auth/login", [](AsyncWebServerRequest* request, JsonVariant& json) {
        const char* pin = json["pin"] | "";
        const uint32_t now = millis();
        DynamicJsonDocument doc(256);
        if (antcore_auth::lockedOut(webState.authState, now)) {
          doc["ok"] = false;
          doc["error"] = "locked";
          doc["reason"] = "too many failed attempts";
          doc["retryMs"] = antcore_auth::lockoutRemainingMs(webState.authState, now);
          sendJson(request, doc, 429);
          addLog("WARN", "admin login locked out");
          return;
        }
        const bool ok = antcore_auth::adminPinIsValid(configState.cfg, pin);
        doc["ok"] = ok;
        if (ok) {
          antcore_auth::generateSessionToken(webState.authState);
          antcore_auth::recordSuccess(webState.authState);
          doc["token"] = antcore_auth::token(webState.authState);
          doc["authEnabled"] = configState.cfg.security.authEnabled;
          doc["defaultPin"] = isDefaultAdminPin();
          addLog("INFO", "admin session opened");
        } else {
          antcore_auth::recordFailure(webState.authState);
          doc["error"] = "invalid_pin";
          doc["reason"] = "invalid admin PIN";
          addLog("WARN", "admin login rejected");
        }
        String out;
        serializeJson(doc, out);
        AsyncWebServerResponse* response =
            request->beginResponse(ok ? 200 : 403, "application/json", out);
        response->addHeader("Cache-Control", "no-store");
        if (ok) response->addHeader("Set-Cookie", antcore_auth::tokenCookieHeader(webState.authState));
        request->send(response);
      }, 512);
  loginHandler->setMethod(HTTP_POST);
  webState.server.addHandler(loginHandler);

  webState.server.on("/api/auth/logout", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    antcore_auth::clearSession(webState.authState);
    RuntimeCommand command;
    command.type = RuntimeCommandType::ReleaseWebControl;
    command.source = "logout";
    submitRuntimeCommand(command);
    DynamicJsonDocument doc(128);
    doc["ok"] = true;
    doc["message"] = "logged out";
    String out;
    serializeJson(doc, out);
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", out);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("Set-Cookie", antcore_auth::clearTokenCookieHeader());
    request->send(response);
    addLog("INFO", "admin session closed");
  });
}

}  // namespace antcore_app
