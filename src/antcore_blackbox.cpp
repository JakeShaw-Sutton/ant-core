#include "antcore_file_store.h"
#include "antcore_blackbox.h"

#include <LittleFS.h>

#include "antcore_firmware_config.h"

namespace antcore_blackbox {

namespace {

constexpr size_t BLACKBOX_COPY_BUFFER_BYTES = 512;



bool rewriteTrimmedLog(File& current, size_t start, uint32_t nowMs) {
  const String tmp = String(BLACKBOX_LOG_PATH) + ".tmp";
  LittleFS.remove(tmp);
  File replacement = LittleFS.open(tmp, FILE_WRITE);
  if (!replacement) return false;
  if (!replacement.setBufferSize(BLACKBOX_COPY_BUFFER_BYTES)) {
    replacement.close();
    LittleFS.remove(tmp);
    return false;
  }

  const String header = String(nowMs) + " INFO blackbox truncated\r\n";
  if (replacement.print(header) != header.length()) {
    replacement.close();
    LittleFS.remove(tmp);
    return false;
  }

  if (!current.seek(start)) {
    replacement.close();
    LittleFS.remove(tmp);
    return false;
  }

  uint8_t buffer[BLACKBOX_COPY_BUFFER_BYTES];
  while (current.available()) {
    const size_t available = static_cast<size_t>(current.available());
    const size_t requested = available < sizeof(buffer) ? available : sizeof(buffer);
    const int read = current.read(buffer, requested);
    if (read <= 0) {
      replacement.close();
      LittleFS.remove(tmp);
      return false;
    }
    if (replacement.write(buffer, static_cast<size_t>(read)) != static_cast<size_t>(read)) {
      replacement.close();
      LittleFS.remove(tmp);
      return false;
    }
  }

  replacement.close();
  current.close();
  return antcore_storage::commitReplacement(LittleFS, tmp.c_str(), BLACKBOX_LOG_PATH);
}

}  // namespace

void trimLogIfNeeded(bool fsMounted, uint32_t nowMs) {
  if (!fsMounted || !LittleFS.exists(BLACKBOX_LOG_PATH)) return;
  File current = LittleFS.open(BLACKBOX_LOG_PATH, FILE_READ);
  if (!current) return;
  if (!current.setBufferSize(BLACKBOX_COPY_BUFFER_BYTES)) {
    current.close();
    return;
  }
  const size_t size = current.size();
  if (size <= BLACKBOX_LOG_MAX_BYTES) {
    current.close();
    return;
  }
  const size_t start = size > BLACKBOX_LOG_KEEP_BYTES ? size - BLACKBOX_LOG_KEEP_BYTES : 0;
  rewriteTrimmedLog(current, start, nowMs);
  current.close();
}

void appendLog(bool fsMounted, const String& line, uint32_t nowMs) {
  if (!fsMounted) return;
  trimLogIfNeeded(fsMounted, nowMs);
  File file = LittleFS.open(BLACKBOX_LOG_PATH, FILE_APPEND);
  if (!file) return;
  if (!file.setBufferSize(BLACKBOX_COPY_BUFFER_BYTES)) {
    file.close();
    return;
  }
  file.println(line);
  file.close();
}

size_t logSize(bool fsMounted) {
  if (!fsMounted || !LittleFS.exists(BLACKBOX_LOG_PATH)) return 0;
  File file = LittleFS.open(BLACKBOX_LOG_PATH, FILE_READ);
  if (!file) return 0;
  if (!file.setBufferSize(BLACKBOX_COPY_BUFFER_BYTES)) {
    file.close();
    return 0;
  }
  const size_t size = file.size();
  file.close();
  return size;
}

bool readLog(bool fsMounted, String& out) {
  out = "";
  if (!fsMounted || !LittleFS.exists(BLACKBOX_LOG_PATH)) return true;
  File file = LittleFS.open(BLACKBOX_LOG_PATH, FILE_READ);
  if (!file) return false;
  if (!file.setBufferSize(BLACKBOX_COPY_BUFFER_BYTES)) {
    file.close();
    return false;
  }
  out.reserve(file.size() + 1);
  while (file.available()) {
    out += static_cast<char>(file.read());
  }
  file.close();
  return true;
}

bool clearLog(bool fsMounted) {
  return !fsMounted || !LittleFS.exists(BLACKBOX_LOG_PATH) || LittleFS.remove(BLACKBOX_LOG_PATH);
}

}  // namespace antcore_blackbox
