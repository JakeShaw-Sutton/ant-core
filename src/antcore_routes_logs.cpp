#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/web_state.h"
#include "antcore_app_routes.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_log.h"
#include "antcore_blackbox.h"
#include "antcore_http_response.h"
#include <LittleFS.h>
#include <new>

namespace antcore_app {

void registerLogsRoutes() {
  webState.server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    DynamicJsonDocument doc(LOG_JSON_CAPACITY);
    JsonArray logs = doc.createNestedArray("logs");
    addLogsToJson(logs, API_LOG_MAX_LINES);
    doc["blackboxBytes"] = blackboxLogSize();
    doc["blackboxAvailable"] = configState.fsMounted;
    sendJson(request, doc);
  });

  webState.server.on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    clearLogRing();
    DynamicJsonDocument doc(128);
    doc["ok"] = true;
    doc["message"] = "logs cleared";
    sendJson(request, doc);
  });

  webState.server.on("/api/blackbox", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    if (commandsState.filesystemOtaInProgress) {
      request->send(503, "text/plain", "filesystem OTA in progress");
      return;
    }
    if (!configState.fsMounted) {
      request->send(503, "text/plain", "blackbox storage unavailable");
      return;
    }
    if (!LittleFS.exists(BLACKBOX_LOG_PATH)) {
      request->send(404, "text/plain", "blackbox log not found");
      return;
    }
    // Stream from LittleFS in network-sized chunks. The complete log can be
    // larger than the largest available C3 heap block.
    AsyncWebServerResponse* response = new (std::nothrow)
        antcore::BoundedFileResponse(LittleFS.open(BLACKBOX_LOG_PATH, "r"), "text/plain");
    if (response == nullptr || !response->_sourceValid()) {
      delete response;
      request->send(500, "text/plain", "blackbox log could not be opened");
      return;
    }
    response->addHeader("Content-Disposition", "attachment; filename=\"blackbox.log\"");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  webState.server.on("/api/blackbox/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    if (commandsState.filesystemOtaInProgress) {
      DynamicJsonDocument doc(160);
      doc["ok"] = false;
      doc["message"] = "filesystem OTA in progress";
      sendJson(request, doc, 503);
      return;
    }
    const bool ok = antcore_blackbox::clearLog(configState.fsMounted);
    clearLogRing();
    DynamicJsonDocument doc(160);
    doc["ok"] = ok;
    doc["message"] = ok ? "blackbox log cleared" : "blackbox clear failed";
    sendJson(request, doc, ok ? 200 : 500);
    addLog(ok ? "INFO" : "ERROR", ok ? "blackbox log cleared" : "blackbox clear failed", false);
  });
}

}  // namespace antcore_app
