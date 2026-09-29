#pragma once

#include <DNSServer.h>

#include "antcore_app_network.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct NetworkState {
  DNSServer captiveDns;
  String apSsid = "AntCore";
  uint32_t lastStaAttemptMs = 0;
  bool staEverConnected = false;
  bool staWasConnected = false;
  bool staRestartPending = false;
  bool staConnectTimeoutLogged = false;
  uint32_t staRestartAtMs = 0;
  bool mdnsReady = false;
  bool captiveDnsReady = false;
};

extern NetworkState networkState;

}  // namespace antcore_app
