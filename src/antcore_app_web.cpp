#include "antcore_app_routes.h"
#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/inputs_state.h"
#include "app/lifecycle_state.h"
#include "app/network_state.h"
#include "app/peripherals_state.h"
#include "app/web_state.h"
#include "antcore_app_web.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_inputs.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_log.h"
#include "antcore_app_network.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_telemetry.h"
#include <AsyncJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include "antcore_network.h"
#include "antcore_blackbox.h"
#include "antcore_packs.h"
#include "antcore_logic.h"
#include "antcore_http_response.h"
#include <new>
#include <utility>

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

WebState webState;

static void sendOwnedJson(AsyncWebServerRequest* request, String&& json, int code) {
  auto* response = new (std::nothrow) antcore::OwnedJsonResponse(code, std::move(json));
  if (!response || !response->_sourceValid()) {
    delete response;
    request->send(503, "application/json", "{\"ok\":false,\"message\":\"response memory unavailable\"}");
    return;
  }
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

void sendCachedJson(AsyncWebServerRequest* request, String json) {
  sendOwnedJson(request, std::move(json), 200);
}

static void sendCaptiveRedirect(AsyncWebServerRequest* request);
static bool shouldCaptiveRedirect(AsyncWebServerRequest* request);
static String fallbackHtml();
static void sendFileOrFallback(AsyncWebServerRequest* request, const char* path, const char* type);
static void redirectToCameraServer(AsyncWebServerRequest* request, const char* path);
static void registerCaptiveProbeRoute(const char* path);
static void registerCaptivePortalRoutes();

bool isDefaultAdminPin() {
  return antcore_auth::isDefaultAdminPin(configState.cfg);
}

bool requireAuth(AsyncWebServerRequest* request) {
  if (antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request))) return true;
  antcore_auth::recordFailure(webState.authState);
  antcore_auth::sendAuthFailure(request);
  addLog("WARN", "auth rejected for " + request->url());
  return false;
}

void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code) {
  if (doc.overflowed()) {
    request->send(500, "application/json", "{\"ok\":false,\"message\":\"json response overflow\"}");
    addLog("ERROR", "json response overflow: " + request->url());
    return;
  }
  String out;
  const size_t length = measureJson(doc);
  if (!out.reserve(length) || serializeJson(doc, out) != length || out.length() != length) {
    request->send(503, "application/json", "{\"ok\":false,\"message\":\"response memory unavailable\"}");
    return;
  }
  sendOwnedJson(request, std::move(out), code);
}

void sendRuntimeCommandJson(AsyncWebServerRequest* request, RuntimeCommand& command) {
  DynamicJsonDocument doc(256);
  doc["ok"] = command.ok;
  doc["message"] = command.message;
  doc["reason"] = command.message;
  doc["armed"] = command.armedResult;
  doc["weaponArmed"] = command.weaponArmedResult;
  doc["liveOutputEnabled"] = command.boolResult;
  if (command.type == RuntimeCommandType::PitSet) doc["pitMode"] = command.boolValue;
  if (command.type == RuntimeCommandType::BleForget && command.ok) doc["scanEnabled"] = true;
  if (command.name.length() > 0) doc["name"] = command.name;
  if (command.index > 0) {
    if (command.type == RuntimeCommandType::MotorTest) doc["motor"] = command.index;
    if (command.type == RuntimeCommandType::ServoTest) doc["servo"] = command.index;
  }
  sendJson(request, doc, command.status);
}

String cameraUrlForIp(IPAddress ip, const char* path) {
  return antcore_network::cameraUrlForIp(peripheralsState.cameraReady, ip, path);
}

static void sendCaptiveRedirect(AsyncWebServerRequest* request) {
  antcore_network::sendCaptiveRedirect(request, WiFi.softAPIP());
}

static bool shouldCaptiveRedirect(AsyncWebServerRequest* request) {
  return antcore_network::shouldCaptiveRedirect(request, networkState.captiveDnsReady, WiFi.softAPIP(), staConnected(),
                                                WiFi.localIP(), MDNS_HOSTNAME);
}

static String fallbackHtml() {
  return F("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
           "<title>Ant Core</title><style>body{font-family:system-ui;background:#101418;color:#edf2f4;"
           "margin:2rem;line-height:1.5}.box{max-width:720px}code{background:#202830;padding:.2rem .4rem;"
           "border-radius:4px}a{color:#5eead4}</style><div class=box><h1>Ant Core</h1>"
           "<p>LittleFS web assets are missing. Upload the filesystem image from PlatformIO, then reload.</p>"
           "<p>API is alive at <code>/api/status</code>. OTA firmware uploads are available at "
           "<code>/api/ota/firmware</code>.</p></div>");
}

static void sendFileOrFallback(AsyncWebServerRequest* request, const char* path, const char* type) {
  if (configState.fsMounted && LittleFS.exists(path)) {
    auto* response = new (std::nothrow) antcore::BoundedFileResponse(LittleFS.open(path, "r"), type);
    if (!response || !response->_sourceValid()) {
      delete response;
      request->send(503, "text/plain", "asset unavailable");
      return;
    }
    request->send(response);
  } else if (!strcmp(path, "/index.html")) {
    request->send(200, "text/html", fallbackHtml());
  } else {
    request->send(404, "text/plain", "missing asset");
  }
}

static void redirectToCameraServer(AsyncWebServerRequest* request, const char* path) {
  String host = antcore_network::requestHostWithoutPort(request, WiFi.softAPIP());
  request->redirect(String("http://") + host + ":81" + path);
}

static void registerCaptiveProbeRoute(const char* path) {
  webState.server.on(path, HTTP_GET, [](AsyncWebServerRequest* request) {
    sendCaptiveRedirect(request);
  });
}

static void registerCaptivePortalRoutes() {
  registerCaptiveProbeRoute("/generate_204");
  registerCaptiveProbeRoute("/gen_204");
  registerCaptiveProbeRoute("/hotspot-detect.html");
  registerCaptiveProbeRoute("/library/test/success.html");
  registerCaptiveProbeRoute("/connecttest.txt");
  registerCaptiveProbeRoute("/ncsi.txt");
  registerCaptiveProbeRoute("/redirect");
  registerCaptiveProbeRoute("/fwlink");
  registerCaptiveProbeRoute("/success.txt");
  registerCaptiveProbeRoute("/canonical.html");
}

void registerApiRoutes() {
  registerCaptivePortalRoutes();

  webState.server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendFileOrFallback(request, "/index.html", "text/html");
  });
  webState.server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendFileOrFallback(request, "/index.html", "text/html");
  });
  webState.server.on("/app.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendFileOrFallback(request, "/app.css", "text/css");
  });
  webState.server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendFileOrFallback(request, "/app.js", "application/javascript");
  });

  webState.server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    const bool includeSensitive =
        antcore_auth::tokenIsValid(configState.cfg, webState.authState, antcore_auth::tokenFromRequest(request));
    sendCachedJson(request, cachedStatusJson(includeSensitive));
  });

  registerAuthRoutes();

  registerConfigurationRoutes();

  registerProfilesRoutes();

  registerControlRoutes();

  registerLogsRoutes();

  registerOutputTestsRoutes();

  webState.server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    if (peripheralsState.cameraReady) {
      redirectToCameraServer(request, "/stream");
    } else {
      request->send(503, "text/plain", "camera not available");
    }
  });
  webState.server.on("/snapshot.jpg", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!requireAuth(request)) return;
    if (peripheralsState.cameraReady) {
      redirectToCameraServer(request, "/snapshot.jpg");
    } else {
      request->send(503, "text/plain", "camera not available");
    }
  });


  registerOtaRoutes();

  webState.server.onNotFound([](AsyncWebServerRequest* request) {
    if (shouldCaptiveRedirect(request)) {
      sendCaptiveRedirect(request);
      return;
    }
    DynamicJsonDocument doc(160);
    doc["ok"] = false;
    doc["error"] = "not found";
    doc["path"] = request->url();
    sendJson(request, doc, 404);
  });
}

}  // namespace antcore_app
