#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace antcore_blackbox {

void trimLogIfNeeded(bool fsMounted, uint32_t nowMs);
void appendLog(bool fsMounted, const String& line, uint32_t nowMs);
size_t logSize(bool fsMounted);
bool readLog(bool fsMounted, String& out);
bool clearLog(bool fsMounted);

}  // namespace antcore_blackbox
