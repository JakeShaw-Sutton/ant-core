#include "antcore_network.h"

namespace antcore_network {

String cameraUrlForIp(bool cameraReady, IPAddress ip, const char* path) {
  if (!cameraReady) return "";
  return String("http://") + ip.toString() + ":81" + path;
}

String requestHostWithoutPort(AsyncWebServerRequest* request, IPAddress fallbackIp) {
  String host = request->host();
  int portStart = host.indexOf(':');
  if (portStart >= 0) {
    host = host.substring(0, portStart);
  }
  if (host.length() == 0) {
    host = fallbackIp.toString();
  }
  return host;
}

bool isBoardHost(const String& host, IPAddress apIp, bool staConnected, IPAddress staIp,
                 const char* mdnsHostname) {
  if (host.length() == 0) return true;
  if (host == apIp.toString()) return true;
  if (staConnected && host == staIp.toString()) return true;
  String mdnsHost = String(mdnsHostname) + ".local";
  return host.equalsIgnoreCase(mdnsHost) || host.equalsIgnoreCase(mdnsHostname);
}

bool isCaptiveProbePath(const String& path) {
  return path == "/generate_204" || path == "/gen_204" || path == "/hotspot-detect.html" ||
         path == "/library/test/success.html" || path == "/connecttest.txt" || path == "/ncsi.txt" ||
         path == "/redirect" || path == "/fwlink" || path == "/success.txt" || path == "/canonical.html";
}

void sendCaptiveRedirect(AsyncWebServerRequest* request, IPAddress apIp) {
  String location = String("http://") + apIp.toString() + "/";
  AsyncWebServerResponse* response = request->beginResponse(302, "text/plain", "Ant Core setup portal");
  response->addHeader("Location", location);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

bool shouldCaptiveRedirect(AsyncWebServerRequest* request, bool captiveDnsReady, IPAddress apIp,
                           bool staConnected, IPAddress staIp, const char* mdnsHostname) {
  if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) return false;
  String path = request->url();
  if (isCaptiveProbePath(path)) return true;
  if (path.startsWith("/api/") || path == "/ws" || path == "/stream" || path == "/snapshot.jpg" ||
      path == "/app.css" || path == "/app.js") {
    return false;
  }
  return captiveDnsReady &&
         !isBoardHost(requestHostWithoutPort(request, apIp), apIp, staConnected, staIp, mdnsHostname);
}

}  // namespace antcore_network
