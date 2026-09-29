#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace antcore {

// One complete RFC6455 fragment is offered to TCP atomically. In particular,
// allocation failure cannot leave a header on the wire without its payload.
class WsFrameSender {
 public:
  // A 2KB fragment spans two TCP segments on ESP32. With TCP_NODELAY this
  // avoids one delayed-ACK wait for every sub-MSS kilobyte of telemetry.
  static constexpr size_t MAX_FRAGMENT = 2048;

  WsFrameSender(const char* data, size_t size) : data_(data), size_(size) {}

  bool finished() const { return finished_ || failed_; }
  bool failed() const { return failed_; }
  bool betweenFrames() const { return outstanding_ == 0; }
  bool outputPending() const { return outputPending_; }
  size_t outstandingBytes() const { return outstanding_; }

  void ack(size_t size) {
    outstanding_ -= size < outstanding_ ? size : outstanding_;
    if (outstanding_ == 0) {
      outputPending_ = false;
      if (started_ && sent_ == size_) finished_ = true;
    }
  }

  template <typename TcpClient>
  void retryOutput(TcpClient* client) {
    if (outputPending_ && !finished() && client != nullptr) outputPending_ = !client->send();
  }

  template <typename TcpClient>
  size_t send(TcpClient* client) {
    if (finished() || client == nullptr) return 0;
    if (outputPending_) {
      retryOutput(client);
      return 0;
    }
    if (outstanding_ != 0 || !client->canSend()) return 0;
    const size_t space = client->space();
    if (space < 2) return 0;
    const size_t remaining = size_ - sent_;
    size_t payload = remaining < MAX_FRAGMENT ? remaining : MAX_FRAGMENT;
    const size_t available = space > 4 ? space - 4 : space - 2;
    if (payload > available) payload = available;
    if (remaining && payload == 0) return 0;
    const size_t header = payload > 125 ? 4 : 2;
    uint8_t frame[MAX_FRAGMENT + 4];
    frame[0] = (started_ ? 0x00 : 0x01) | (payload == remaining ? 0x80 : 0x00);
    frame[1] = payload > 125 ? 126 : static_cast<uint8_t>(payload);
    if (header == 4) {
      frame[2] = static_cast<uint8_t>(payload >> 8);
      frame[3] = static_cast<uint8_t>(payload);
    }
    if (payload) std::memcpy(frame + header, data_ + sent_, payload);
    const size_t wireSize = header + payload;
    const size_t accepted = client->add(reinterpret_cast<const char*>(frame), wireSize);
    if (accepted == 0) return 0;
    if (accepted != wireSize) {
      // AsyncTCP normally returns all-or-none when space() is respected. A
      // concurrent window change must terminate this connection, not emit a
      // new frame after a partially accepted one.
      failed_ = true;
      return 0;
    }
    sent_ += payload;
    outstanding_ = wireSize;
    started_ = true;
    // tcp_output may run out of Wi-Fi buffers after TCP has accepted the bytes.
    // Retain that frame and retry output without adding any bytes again. Keep
    // betweenFrames false until its ACK so controls cannot steal its ACK bytes.
    outputPending_ = !client->send();
    return payload;
  }

 private:
  const char* data_;
  size_t size_;
  size_t sent_ = 0;
  size_t outstanding_ = 0;
  bool started_ = false;
  bool finished_ = false;
  bool failed_ = false;
  bool outputPending_ = false;
};

}  // namespace antcore
