"""Exercise production frame emission under TCP allocation/window failures."""
import subprocess
from pathlib import Path
import pytest


ROOT = Path(__file__).resolve().parents[1]


def test_atomic_websocket_fragments(tmp_path):
    executable = tmp_path / "ws-frame-regression.exe"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Iinclude",
                    "test/support/ws_frame_regression.cpp", "-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable)], check=True, timeout=10)


@pytest.mark.parametrize("target", ["s3", "c3"])
def test_websocket_message_lifetime_and_task_races(tmp_path, target):
    # Compile the production adapter with narrow interface doubles. The mutex
    # double is a real host mutex, and close can synchronously delete a message.
    (tmp_path / "Arduino.h").write_text(r'''
#pragma once
#include <string>
using String = std::string;
struct TestEsp {
  size_t freeHeap = 65536, maxAllocHeap = 65536;
  size_t getFreeHeap() const { return freeHeap; }
  size_t getMaxAllocHeap() const { return maxAllocHeap; }
};
inline TestEsp ESP;
''')
    (tmp_path / "freertos").mkdir()
    (tmp_path / "freertos" / "semphr.h").write_text(r'''
#pragma once
#include <cassert>
#include <mutex>
struct StaticSemaphore_t { std::mutex mutex; unsigned active = 0; };
using SemaphoreHandle_t = StaticSemaphore_t*;
constexpr unsigned portMAX_DELAY = ~0u;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* s) { return s; }
inline void xSemaphoreTake(SemaphoreHandle_t s, unsigned) { s->mutex.lock(); ++s->active; }
inline void xSemaphoreGive(SemaphoreHandle_t s) { --s->active; s->mutex.unlock(); }
inline void vSemaphoreDelete(SemaphoreHandle_t s) { assert(s->active == 0); }
''')
    (tmp_path / "ESPAsyncWebServer.h").write_text(r'''
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
struct AsyncClient {
  unsigned addCalls = 0, sendCalls = 0;
  bool partial = false, outputOk = true, closed = false;
  std::function<void()> onClose;
  std::vector<std::vector<char>> writes;
  bool canSend() { return true; }
  size_t space() { return 4096; }
  size_t add(const char* data, size_t length) {
    ++addCalls; writes.emplace_back(data, data + length);
    return partial ? length - 1 : length;
  }
  bool send() { ++sendCalls; return outputOk; }
  void close(bool) { closed = true; if (onClose) onClose(); }
};
struct AsyncWebSocketMessage {
  virtual ~AsyncWebSocketMessage() = default;
  virtual void ack(size_t, uint32_t) {}
  virtual size_t send(AsyncClient*) { return 0; }
  virtual bool finished() { return false; }
  virtual bool betweenFrames() const { return false; }
};
''')
    executable = tmp_path / "ws-message-regression.exe"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    *(["-DCONFIG_IDF_TARGET_ESP32C3=1"] if target == "c3" else []),
                    "-I" + str(tmp_path), "-Iinclude", "test/support/ws_message_regression.cpp",
                    "-o", str(executable)], cwd=ROOT, check=True)
    subprocess.run([str(executable)], check=True, timeout=10)
