#pragma once

#include <Arduino.h>

namespace antcore_app {

bool staConnected();
void startStaConnect();
void restartStaFromConfig();
void scheduleStaRestart();
void servicePendingStaRestart();
void serviceStaReconnect();
void initWiFiAp();
void initMdns();

}  // namespace antcore_app
