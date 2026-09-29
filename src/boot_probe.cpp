#ifdef ANTCORE_BOOT_PROBE

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "antcore_firmware_config.h"

namespace {

WebServer server(80);
String apSsid;
uint32_t bootMs = 0;

String statusJson() {
  String body = "{";
  body += "\"schema\":10,";
  body += "\"probe\":true,";
  body += "\"firmwareVersion\":\"boot-probe\",";
  body += "\"uptimeMs\":" + String(millis() - bootMs) + ",";
  body += "\"resetReason\":" + String(static_cast<int>(esp_reset_reason())) + ",";
  body += "\"ap\":{\"ssid\":\"" + apSsid + "\",\"ip\":\"" + WiFi.softAPIP().toString() + "\",\"clients\":" +
          String(WiFi.softAPgetStationNum()) + "},";
  body += "\"sta\":{\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
          ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  body += "}";
  return body;
}

void sendStatus() {
  server.send(200, "application/json", statusJson());
}

void pollSerial() {
  static String line;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      line.trim();
      if (line == "PING") {
        Serial.println("PONG AntCore boot-probe");
      } else if (line == "STATUS") {
        Serial.print("STATUS ");
        Serial.print(statusJson());
        Serial.println();
      } else if (line == "DISARM") {
        Serial.println("OK boot-probe outputs untouched");
      } else if (line.length() > 0) {
        Serial.println("ERR boot-probe supports PING STATUS DISARM");
      }
      line = "";
    } else if (line.length() < 128) {
      line += c;
    }
  }
}

}  // namespace

void setup() {
  bootMs = millis();
  Serial.begin(SERIAL_BAUD);
  delay(700);
  Serial.println();
  Serial.println("Ant Core boot probe");
  Serial.printf("Reset reason: %d\n", static_cast<int>(esp_reset_reason()));

  uint64_t mac = ESP.getEfuseMac();
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "AntCore-Probe-%04X", static_cast<uint16_t>(mac & 0xFFFF));
  apSsid = ssid;
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(apSsid.c_str(), DEFAULT_AP_PASSWORD, 6, false, 4);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  delay(150);
  Serial.printf("Probe AP %s at %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, sendStatus);
  server.on("/api/status", HTTP_GET, sendStatus);
  server.begin();
  Serial.println("Probe web server ready");
}

void loop() {
  pollSerial();
  server.handleClient();
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 1000) {
    lastBeat = millis();
    Serial.printf("BOOT_PROBE %lu %s\n", static_cast<unsigned long>(millis()), WiFi.softAPIP().toString().c_str());
  }
  delay(2);
}

#endif
