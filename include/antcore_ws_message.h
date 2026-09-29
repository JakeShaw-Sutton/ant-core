#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <freertos/semphr.h>
#include <atomic>
#include <new>
#include <utility>
#include <cstdio>

#include "antcore_ws_frame.h"

namespace antcore {

// Intrusive references avoid allocating a separate shared_ptr control block.
// Moving the already-serialized String avoids a second full telemetry copy.
struct WsPayload {
  explicit WsPayload(String&& text) : text(std::move(text)) { allocatedBytes().fetch_add(this->text.length()); }
  ~WsPayload() { allocatedBytes().fetch_sub(text.length()); }
  String text;
  std::atomic<unsigned> references{1};
  static std::atomic<size_t>& allocatedBytes() { static std::atomic<size_t> bytes{0}; return bytes; }
  void retain() { references.fetch_add(1); }
  void release() { if (references.fetch_sub(1) == 1) delete this; }
};

// Hold across the heap/budget check, snapshot copy and payload publication.
// A competing connect callback skips its initial frame rather than blocking
// AsyncTCP; the next periodic broadcast supplies a fresh snapshot.
class WsStatusAdmission {
 public:
  explicit WsStatusAdmission(std::atomic_flag& gate)
      : gate_(gate), acquired_(!gate.test_and_set(std::memory_order_acquire)) {}
  ~WsStatusAdmission() { if (acquired_) gate_.clear(std::memory_order_release); }
  explicit operator bool() const { return acquired_; }
  WsStatusAdmission(const WsStatusAdmission&) = delete;
  WsStatusAdmission& operator=(const WsStatusAdmission&) = delete;

 private:
  std::atomic_flag& gate_;
  bool acquired_;
};

class WsMessage : public AsyncWebSocketMessage {
 public:
  using Completion = void (*)(uint32_t, bool);
  WsMessage(WsPayload* payload, uint32_t clientId, bool telemetry, Completion complete)
      : payload_(payload), sender_(payload->text.c_str(), payload->text.length()),
        clientId_(clientId), telemetry_(telemetry), complete_(complete),
        mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_)) {
    payload_->retain();
  }
  ~WsMessage() override {
    payload_->release();
    if (complete_) complete_(clientId_, telemetry_);
    vSemaphoreDelete(mutex_);
  }
  void ack(size_t length, uint32_t) override {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    sender_.ack(length);
    xSemaphoreGive(mutex_);
  }
  size_t send(AsyncClient* client) override {
    // Initial enqueue can run on loopTask while ACK/poll callbacks run on the
    // AsyncTCP task. A static mutex protects offsets without heap allocation
    // or spinning at a higher priority than the task holding the lock.
    xSemaphoreTake(mutex_, portMAX_DELAY);
    client_ = client;
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // Retain Wi-Fi receive/ACK headroom before each new TCP allocation, not
    // just before copying a telemetry snapshot. Leave the message queued so
    // the normal ACK/poll path retries when concurrent HTTP traffic drains.
    // 2304 bytes covers the 2052-byte wire frame plus pbuf/header overhead.
    if (sender_.betweenFrames() && !sender_.finished() &&
        (ESP.getFreeHeap() < 16384 || ESP.getMaxAllocHeap() < 2304)) {
      xSemaphoreGive(mutex_);
      return 0;
    }
#endif
    const size_t sent = sender_.send(client);
    const bool failed = sender_.failed();
    const bool reportDeferred = sender_.outputPending() && !reportedDeferred_;
    const size_t deferredBytes = sender_.outstandingBytes();
    if (reportDeferred) reportedDeferred_ = true;
    xSemaphoreGive(mutex_);
#if defined(ARDUINO_ARCH_ESP32)
    if (reportDeferred) {
      char line[112];
      const int length = snprintf(line, sizeof(line), "WS output deferred pending=%u heap=%u max=%u\n",
                                  static_cast<unsigned>(deferredBytes), static_cast<unsigned>(ESP.getFreeHeap()),
                                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
      if (length > 0) Serial.write(reinterpret_cast<const uint8_t*>(line),
                                   static_cast<size_t>(length) < sizeof(line) ? length : sizeof(line) - 1);
    }
#else
    (void)deferredBytes;
#endif
    // close(true) can destroy this message via disconnect callbacks. Do not
    // access members afterwards, and never close while holding its mutex.
    if (failed && client) client->close(true);
    return sent;
  }
  bool finished() override {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool result = sender_.finished();
    xSemaphoreGive(mutex_);
    return result;
  }
  bool betweenFrames() const override {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    // The pinned library only calls send() between frames. Its poll/ACK queue
    // does call this method while one is outstanding, so retry only tcp_output
    // here without admitting another data/control frame or changing offsets.
    sender_.retryOutput(client_);
    const bool result = sender_.betweenFrames();
    xSemaphoreGive(mutex_);
    return result;
  }

 private:
  WsPayload* payload_;
  mutable WsFrameSender sender_;
  AsyncClient* client_ = nullptr;
  bool reportedDeferred_ = false;
  uint32_t clientId_;
  bool telemetry_;
  Completion complete_;
  StaticSemaphore_t mutexStorage_;
  SemaphoreHandle_t mutex_;
};

}  // namespace antcore
