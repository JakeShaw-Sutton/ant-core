#include "app/config_state.h"
#include "app/network_state.h"
#include "antcore_app_network.h"
#include "antcore_app_config.h"
#include "antcore_app_log.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

NetworkState networkState;



bool staConnected() {
  return configState.cfg.wifi.staEnabled && WiFi.status() == WL_CONNECTED;
}

void startStaConnect() {
  if (!configState.cfg.wifi.staEnabled || strlen(configState.cfg.wifi.staSsid) == 0) return;
  // BLE + Wi-Fi coexistence on ESP32-S3 requires modem sleep to remain enabled.
  WiFi.setSleep(true);
  WiFi.mode(WIFI_AP_STA);
  networkState.lastStaAttemptMs = millis();
  networkState.staConnectTimeoutLogged = false;
  WiFi.begin(configState.cfg.wifi.staSsid, configState.cfg.wifi.staPassword);
  addLog("INFO", "STA connecting to " + String(configState.cfg.wifi.staSsid));
}

void restartStaFromConfig() {
  networkState.staEverConnected = false;
  networkState.staWasConnected = false;
  networkState.staConnectTimeoutLogged = false;
  networkState.lastStaAttemptMs = 0;
  networkState.staRestartPending = false;
  WiFi.disconnect(false, false);
  if (configState.cfg.wifi.staEnabled && strlen(configState.cfg.wifi.staSsid) > 0) {
    startStaConnect();
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    addLog("INFO", "STA disabled; AP-only mode");
  }
}

void scheduleStaRestart() {
  networkState.staRestartPending = true;
  networkState.staRestartAtMs = millis() + 500;
}

void servicePendingStaRestart() {
  if (networkState.staRestartPending && static_cast<int32_t>(millis() - networkState.staRestartAtMs) >= 0) {
    restartStaFromConfig();
  }
}

void serviceStaReconnect() {
  if (!configState.cfg.wifi.staEnabled || strlen(configState.cfg.wifi.staSsid) == 0) return;
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t now = millis();

  if (connected) {
    if (!networkState.staWasConnected) {
      addLog("INFO", "STA connected at " + WiFi.localIP().toString());
    }
    networkState.staEverConnected = true;
    networkState.staWasConnected = true;
    networkState.lastStaAttemptMs = now;
    return;
  }

  if (networkState.staWasConnected) {
    addLog("WARN", "STA disconnected; AP remains active");
    networkState.staWasConnected = false;
  }
  if (!networkState.staConnectTimeoutLogged && networkState.lastStaAttemptMs > 0 && now - networkState.lastStaAttemptMs >= STA_CONNECT_TIMEOUT_MS) {
    addLog("WARN", "STA connect timeout; AP remains active");
    networkState.staConnectTimeoutLogged = true;
  }
  if (now - networkState.lastStaAttemptMs >= STA_RECONNECT_MS) {
    startStaConnect();
  }
}

void initWiFiAp() {
  uint64_t mac = ESP.getEfuseMac();
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "AntCore-%04X", static_cast<uint16_t>(mac & 0xFFFF));
  networkState.apSsid = ssid;
  WiFi.mode(configState.cfg.wifi.staEnabled ? WIFI_AP_STA : WIFI_AP);
  // Disabling modem sleep before NimBLE starts can panic in coex_core_enable().
  WiFi.setSleep(true);
  const char* password = strlen(configState.cfg.apPassword) >= 8 ? configState.cfg.apPassword : DEFAULT_AP_PASSWORD;
  bool ok = WiFi.softAP(networkState.apSsid.c_str(), password, 6, false, 4);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  delay(100);
  addLog(ok ? "INFO" : "ERROR", ok ? "AP " + networkState.apSsid + " at " + WiFi.softAPIP().toString() : "AP start failed");
  if (ok) {
    networkState.captiveDns.stop();
    networkState.captiveDns.setTTL(30);
    networkState.captiveDns.setErrorReplyCode(DNSReplyCode::NoError);
    networkState.captiveDnsReady = networkState.captiveDns.start(CAPTIVE_DNS_PORT, "*", WiFi.softAPIP());
    addLog(networkState.captiveDnsReady ? "INFO" : "WARN",
           networkState.captiveDnsReady ? "captive portal DNS ready" : "captive portal DNS start failed");
  } else {
    networkState.captiveDnsReady = false;
  }
}

void initMdns() {
  networkState.mdnsReady = MDNS.begin(MDNS_HOSTNAME);
  if (networkState.mdnsReady) {
    MDNS.addService("http", "tcp", 80);
    addLog("INFO", String("mDNS ready at ") + MDNS_HOSTNAME + ".local");
  } else {
    addLog("WARN", "mDNS start failed");
  }
}

}  // namespace antcore_app
