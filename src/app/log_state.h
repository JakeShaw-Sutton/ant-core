#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "antcore_app_log.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct PendingLogLine {
  char line[LOG_LINE_MAX_LEN] = "";
  bool persist = false;
};

struct LogState {
  String logRing[LOG_RING_SIZE];
  uint8_t logHead = 0;
  uint8_t logCount = 0;
  SemaphoreHandle_t logMutex = nullptr;
  portMUX_TYPE pendingLogMux = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE blackboxMux = portMUX_INITIALIZER_UNLOCKED;
  PendingLogLine pendingLogs[PENDING_LOG_QUEUE_SIZE];
  uint8_t pendingLogHead = 0;
  uint8_t pendingLogTail = 0;
  uint8_t pendingLogCount = 0;
  volatile uint32_t droppedLogLines = 0;
  char pendingBlackboxLines[BLACKBOX_WRITE_QUEUE_SIZE][LOG_LINE_MAX_LEN] = {};
  uint8_t pendingBlackboxHead = 0;
  uint8_t pendingBlackboxTail = 0;
  uint8_t pendingBlackboxCount = 0;
  volatile uint32_t droppedBlackboxLines = 0;
  uint32_t lastBlackboxServiceMs = 0;
};

extern LogState logState;

}  // namespace antcore_app
