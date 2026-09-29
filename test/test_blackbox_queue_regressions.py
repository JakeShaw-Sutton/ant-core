"""Exercise the production queue service across transient heap pressure."""
from pathlib import Path
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("c3", [True, False])
def test_blackbox_heap_backpressure_keeps_pending_lines(tmp_path, c3):
    source = (ROOT / "src/antcore_app_log.cpp").read_text(encoding="utf-8")
    service = source[source.index("void serviceBlackboxQueue("):source.index("void addLogsToJson(")]
    harness = tmp_path / "blackbox-queue.cpp"
    harness.write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
using String = std::string;
struct { bool otaInProgress = false; } commandsState;
struct { bool fsMounted = true; } configState;
struct { uint32_t lastBlackboxServiceMs = 0; } logState;
struct { unsigned free = 16383; unsigned getFreeHeap() { return free; } } ESP;
constexpr unsigned BLACKBOX_SERVICE_INTERVAL_MS = 100;
constexpr unsigned LOG_LINE_MAX_LEN = 192;
unsigned now = 1000, pending = 3, popped = 0, appended = 0;
uint32_t millis() { return now; }
bool popBlackboxLine(char* out, size_t) {
  if (!pending) return false;
  --pending; ++popped; std::strcpy(out, "queued line"); return true;
}
namespace antcore_blackbox {
void appendLog(bool, const String& line, uint32_t) {
  assert(line == "queued line"); ++appended; ESP.free = 16383;
}
}
''' + service + r'''
int main() {
  serviceBlackboxQueue(3);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  assert(pending == 3 && popped == 0 && appended == 0);
  now += 100; ESP.free = 16384;
  serviceBlackboxQueue(3);
  assert(pending == 2 && popped == 1 && appended == 1);
  now += 100; ESP.free = 16384;
  serviceBlackboxQueue(3);
  assert(pending == 1 && popped == 2 && appended == 2);
#else
  assert(pending == 0 && popped == 3 && appended == 3);
#endif
}
''', encoding="utf-8")
    executable = tmp_path / "blackbox-queue.exe"
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror"]
    if c3:
        command.append("-DCONFIG_IDF_TARGET_ESP32C3")
    subprocess.run(command + [str(harness), "-o", str(executable)], check=True, timeout=30)
    subprocess.run([str(executable)], check=True, timeout=10)
