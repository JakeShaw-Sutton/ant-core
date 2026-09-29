#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

namespace antcore_network {

String cameraUrlForIp(bool cameraReady, IPAddress ip, const char* path);
String requestHostWithoutPort(AsyncWebServerRequest* request, IPAddress fallbackIp);
bool isBoardHost(const String& host, IPAddress apIp, bool staConnected, IPAddress staIp,
                 const char* mdnsHostname);
bool isCaptiveProbePath(const String& path);
void sendCaptiveRedirect(AsyncWebServerRequest* request, IPAddress apIp);
bool shouldCaptiveRedirect(AsyncWebServerRequest* request, bool captiveDnsReady, IPAddress apIp,
                           bool staConnected, IPAddress staIp, const char* mdnsHostname);

}  // namespace antcore_network
