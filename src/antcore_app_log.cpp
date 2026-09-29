#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/log_state.h"
#include "app/ws_state.h"
#include "antcore_app_log.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_ws.h"
#include "antcore_blackbox.h"
#include "antcore_logic.h"
#include <utility>

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

LogState logState;

static bool enqueueBlackboxLine(const char* line);
static bool popBlackboxLine(char* line, size_t lineLen);
static void appendBlackboxLog(const String& line);
static bool enqueueLogLine(const char* line, bool persist);
static bool popLogLine(PendingLogLine& line);
static void commitLogLine(const char* line, bool persist);



static bool enqueueBlackboxLine(const char* line) {
  if (line == nullptr || line[0] == '\0') return true;
  bool queued = false;
  portENTER_CRITICAL(&logState.blackboxMux);
  if (logState.pendingBlackboxCount < BLACKBOX_WRITE_QUEUE_SIZE) {
    strlcpy(logState.pendingBlackboxLines[logState.pendingBlackboxTail], line, LOG_LINE_MAX_LEN);
    logState.pendingBlackboxTail = (logState.pendingBlackboxTail + 1) % BLACKBOX_WRITE_QUEUE_SIZE;
    logState.pendingBlackboxCount++;
    queued = true;
  } else {
    logState.droppedBlackboxLines++;
  }
  portEXIT_CRITICAL(&logState.blackboxMux);
  return queued;
}

static bool popBlackboxLine(char* line, size_t lineLen) {
  bool popped = false;
  portENTER_CRITICAL(&logState.blackboxMux);
  if (logState.pendingBlackboxCount > 0) {
    strlcpy(line, logState.pendingBlackboxLines[logState.pendingBlackboxHead], lineLen);
    logState.pendingBlackboxHead = (logState.pendingBlackboxHead + 1) % BLACKBOX_WRITE_QUEUE_SIZE;
    logState.pendingBlackboxCount--;
    popped = true;
  }
  portEXIT_CRITICAL(&logState.blackboxMux);
  return popped;
}

static void appendBlackboxLog(const String& line) {
  enqueueBlackboxLine(line.c_str());
}

size_t blackboxLogSize() {
  return antcore_blackbox::logSize(configState.fsMounted);
}

static bool enqueueLogLine(const char* line, bool persist) {
  bool queued = false;
  portENTER_CRITICAL(&logState.pendingLogMux);
  if (logState.pendingLogCount < PENDING_LOG_QUEUE_SIZE) {
    strlcpy(logState.pendingLogs[logState.pendingLogTail].line, line, LOG_LINE_MAX_LEN);
    logState.pendingLogs[logState.pendingLogTail].persist = persist;
    logState.pendingLogTail = (logState.pendingLogTail + 1) % PENDING_LOG_QUEUE_SIZE;
    logState.pendingLogCount++;
    queued = true;
  } else {
    logState.droppedLogLines++;
  }
  portEXIT_CRITICAL(&logState.pendingLogMux);
  return queued;
}

static bool popLogLine(PendingLogLine& line) {
  bool popped = false;
  portENTER_CRITICAL(&logState.pendingLogMux);
  if (logState.pendingLogCount > 0) {
    line = logState.pendingLogs[logState.pendingLogHead];
    logState.pendingLogHead = (logState.pendingLogHead + 1) % PENDING_LOG_QUEUE_SIZE;
    logState.pendingLogCount--;
    popped = true;
  }
  portEXIT_CRITICAL(&logState.pendingLogMux);
  return popped;
}

static void commitLogLine(const char* line, bool persist) {
  if (line == nullptr || line[0] == '\0') return;
  if (logState.logMutex) xSemaphoreTake(logState.logMutex, portMAX_DELAY);
  logState.logRing[logState.logHead] = line;
  logState.logHead = (logState.logHead + 1) % LOG_RING_SIZE;
  if (logState.logCount < LOG_RING_SIZE) logState.logCount++;
  if (logState.logMutex) xSemaphoreGive(logState.logMutex);

  Serial.print("LOG ");
  Serial.println(line);
  if (persist) appendBlackboxLog(String(line));

  DynamicJsonDocument doc(320);
  doc["type"] = "log";
  doc["line"] = line;
  String out;
  if (!doc.overflowed()) {
    serializeJson(doc, out);
    broadcastWebSocketText(std::move(out));
  }
}

void serviceLogQueue(uint8_t maxLines) {
  PendingLogLine line;
  for (uint8_t i = 0; i < maxLines && popLogLine(line); i++) {
    commitLogLine(line.line, line.persist);
  }
}

void serviceBlackboxQueue(uint8_t maxLines) {
  if (commandsState.otaInProgress) return;
  const uint32_t now = millis();
  if (now - logState.lastBlackboxServiceMs < BLACKBOX_SERVICE_INTERVAL_MS) return;
  logState.lastBlackboxServiceMs = now;
  char line[LOG_LINE_MAX_LEN];
  for (uint8_t i = 0; i < maxLines; i++) {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // Keep queued lines intact until TCP/Wi-Fi have released their buffers.
    if (ESP.getFreeHeap() < 16384) return;
#endif
    if (!popBlackboxLine(line, sizeof(line))) break;
    antcore_blackbox::appendLog(configState.fsMounted, String(line), now);
  }
}

void addLogsToJson(JsonArray logs, uint8_t maxLines) {
  if (logState.logMutex) xSemaphoreTake(logState.logMutex, portMAX_DELAY);
  const uint8_t count = logState.logCount < maxLines ? logState.logCount : maxLines;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (logState.logHead + LOG_RING_SIZE - count + i) % LOG_RING_SIZE;
    logs.add(logState.logRing[idx]);
  }
  if (logState.logMutex) xSemaphoreGive(logState.logMutex);
}

void clearLogRing() {
  if (logState.logMutex) xSemaphoreTake(logState.logMutex, portMAX_DELAY);
  for (uint8_t i = 0; i < LOG_RING_SIZE; i++) logState.logRing[i] = "";
  logState.logHead = 0;
  logState.logCount = 0;
  if (logState.logMutex) xSemaphoreGive(logState.logMutex);
}

void addLog(const char* level, const String& message, bool persist) {
  char line[LOG_LINE_MAX_LEN];
  snprintf(line, sizeof(line), "%lu %s %s", static_cast<unsigned long>(millis()),
           level == nullptr ? "INFO" : level, message.c_str());
  if (!enqueueLogLine(line, persist)) {
    Serial.print("LOG ");
    Serial.println(line);
  }
}

}  // namespace antcore_app
