#include "app/web_state.h"
#include "app/config_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include "antcore_app_telemetry.h"
#include "antcore_request_body.h"

namespace antcore_app {

namespace {
constexpr size_t kConfigBodyLimit = 16384;

void receiveConfigBody(AsyncWebServerRequest* request, uint8_t* data, size_t length,
                       size_t index, size_t total) {
  // Authentication failures are answered once, in the completed request callback.
  if (!antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) return;
  if (!request->contentType().equalsIgnoreCase("application/json")) return;
  antcore::appendRequestBody(request->_tempObject, data, length, index, total, kConfigBodyLimit);
}

void applyConfigRequest(AsyncWebServerRequest* request) {
  if (!requireAuth(request)) {
    antcore::releaseRequestBody(request->_tempObject);
    return;
  }
  if (!request->contentType().equalsIgnoreCase("application/json")) {
    antcore::releaseRequestBody(request->_tempObject);
    request->send(415, "application/json", "{\"ok\":false,\"message\":\"application/json required\"}");
    return;
  }
  if (request->contentLength() == 0 || request->contentLength() > kConfigBodyLimit) {
    antcore::releaseRequestBody(request->_tempObject);
    request->send(request->contentLength() ? 413 : 400, "application/json",
                  "{\"ok\":false,\"message\":\"invalid config body length\"}");
    return;
  }
  auto* body = static_cast<antcore::RequestBody*>(request->_tempObject);
  if (!body) {
    request->send(503, "application/json", "{\"ok\":false,\"message\":\"config request memory unavailable\"}");
    return;
  }
  if (!body->valid || body->received != body->expected || body->expected != request->contentLength()) {
    antcore::releaseRequestBody(request->_tempObject);
    request->send(400, "application/json", "{\"ok\":false,\"message\":\"incomplete config body\"}");
    return;
  }
  RuntimeCommand command;
  command.type = RuntimeCommandType::ApplyConfig;
  command.source = "config save";
  const size_t length = body->received;
  const bool copied = command.text.reserve(length) && command.text.concat(body->data(), length) &&
                      command.text.length() == length;
  // Free the body before the loop allocates its single JSON parse document.
  antcore::releaseRequestBody(request->_tempObject);
  if (!copied) {
    request->send(503, "application/json", "{\"ok\":false,\"message\":\"config request memory unavailable\"}");
    return;
  }
  submitRuntimeCommand(command);
  sendRuntimeCommandJson(request, command);
}
}  // namespace

void registerConfigurationRoutes() {
  webState.server.on("/api/config/validate", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendCachedJson(request, cachedValidationJson());
  });

  webState.server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendCachedJson(request, cachedConfigJson());
  });

  webState.server.on("/api/config/export", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendCachedJson(request, cachedConfigJson());
  });

  webState.server.on("/api/config", HTTP_PUT, applyConfigRequest, nullptr, receiveConfigBody);

  webState.server.on("/api/config/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::ResetConfig;
    command.source = "factory reset";
    submitRuntimeCommand(command);
    sendRuntimeCommandJson(request, command);
  });
}

}  // namespace antcore_app
