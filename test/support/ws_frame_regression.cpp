#include "antcore_ws_frame.h"

#include <cassert>
#include <string>
#include <vector>

struct Tcp {
  size_t room = 4096;
  unsigned addFailures = 0;
  unsigned sendFailures = 0;
  unsigned addCalls = 0;
  unsigned sendCalls = 0;
  bool partial = false;
  bool closed = false;
  std::vector<std::vector<uint8_t>> writes;
  bool canSend() { return !closed; }
  size_t space() { return room; }
  size_t add(const char* bytes, size_t size) {
    ++addCalls;
    assert(size <= room);
    assert(size <= antcore::WsFrameSender::MAX_FRAGMENT + 4);
    if (addFailures) { --addFailures; return 0; }
    const size_t accepted = partial ? size - 1 : size;
    writes.emplace_back(bytes, bytes + accepted);
    return accepted;
  }
  bool send() { ++sendCalls; if (sendFailures) { --sendFailures; return false; } return true; }
  void close(bool) { closed = true; }
};

std::string reconstruct(const Tcp& tcp) {
  std::string result;
  for (size_t i = 0; i < tcp.writes.size(); ++i) {
    const auto& frame = tcp.writes[i];
    assert(frame.size() >= 2);
    assert((frame[0] & 0x70) == 0);  // no reserved bits
    assert((frame[1] & 0x80) == 0);  // server frames are unmasked
    assert((frame[0] & 0x0f) == (i == 0 ? 1 : 0));
    assert(bool(frame[0] & 0x80) == (i + 1 == tcp.writes.size()));
    const size_t header = frame[1] == 126 ? 4 : 2;
    const size_t length = header == 4 ? (size_t(frame[2]) << 8) | frame[3] : frame[1];
    assert(length + header == frame.size());
    result.append(reinterpret_cast<const char*>(frame.data() + header), length);
  }
  return result;
}

int main() {
  const std::string payload = "{\"value\":\"" + std::string(9000, 'x') + "\"}";
  {
    const std::string status(6322, 's');
    Tcp tcp;
    antcore::WsFrameSender sender(status.data(), status.size());
    assert(sender.send(&tcp) == 2048);
    assert(tcp.writes.size() == 1 && tcp.writes[0].size() == 2052);
    sender.ack(1436);  // first TCP segment alone cannot release the fragment
    sender.send(&tcp);
    assert(tcp.addCalls == 1 && !sender.betweenFrames());
    sender.ack(616);
    while (!sender.finished()) {
      sender.send(&tcp);
      sender.ack(tcp.writes.back().size());
    }
    assert(tcp.writes.size() == 4 && reconstruct(tcp) == status);
  }
  for (size_t room : {size_t(3), size_t(127), size_t(1028), size_t(4096)}) {
    Tcp tcp;
    tcp.room = room;
    antcore::WsFrameSender sender(payload.data(), payload.size());
    unsigned iterations = 0;
    while (!sender.finished()) {
      assert(++iterations < 20000);
      const size_t before = tcp.writes.size();
      sender.send(&tcp);
      assert(tcp.writes.size() == before + 1);
      const auto wireSize = tcp.writes.back().size();
      assert(!sender.betweenFrames());
      sender.ack(1);
      sender.send(&tcp);
      assert(tcp.writes.size() == before + 1);  // partial ACK cannot advance
      sender.ack(wireSize - 1);
    }
    assert(reconstruct(tcp) == payload);
  }
  {
    Tcp tcp;
    tcp.addFailures = 2;
    antcore::WsFrameSender sender(payload.data(), payload.size());
    assert(sender.send(&tcp) == 0);
    assert(sender.send(&tcp) == 0);
    assert(tcp.writes.empty() && sender.betweenFrames() && !sender.finished());
    while (!sender.finished()) {
      sender.send(&tcp);
      sender.ack(tcp.writes.back().size());
    }
    assert(reconstruct(tcp) == payload);
  }
  {
    const std::string small = "hello";
    Tcp tcp;
    tcp.sendFailures = 1;
    antcore::WsFrameSender sender(small.data(), small.size());
    assert(sender.send(&tcp) == small.size());
    assert(!sender.betweenFrames());  // never expose a frame before its ACK
    assert(!sender.failed() && sender.outputPending());
    assert(sender.send(&tcp) == 0);
    assert(!sender.outputPending() && !sender.betweenFrames());
    assert(sender.send(&tcp) == 0);
    assert(tcp.addCalls == 1 && tcp.sendCalls == 2 && !sender.finished());
    sender.ack(small.size() + 2);
    assert(sender.finished());
    assert(reconstruct(tcp) == small);
  }
  {
    Tcp tcp;
    tcp.sendFailures = 2;
    antcore::WsFrameSender sender(payload.data(), payload.size());
    sender.send(&tcp);
    sender.ack(1436); // output may partly succeed before reporting congestion
    sender.retryOutput(&tcp);
    assert(sender.outputPending() && !sender.betweenFrames() && tcp.addCalls == 1);
    sender.ack(616); // a complete ACK supersedes pending output before retry
    assert(!sender.outputPending() && sender.betweenFrames());
    const auto sends = tcp.sendCalls;
    sender.retryOutput(&tcp);
    assert(tcp.sendCalls == sends);
    while (!sender.finished()) {
      sender.send(&tcp);
      sender.ack(tcp.writes.back().size());
    }
    assert(reconstruct(tcp) == payload);
  }
  {
    Tcp tcp;
    tcp.partial = true;
    antcore::WsFrameSender sender(payload.data(), payload.size());
    sender.send(&tcp);
    assert(sender.failed() && sender.finished());
    sender.send(&tcp);
    assert(tcp.addCalls == 1);  // never append a new header after a partial write
  }
  {
    Tcp tcp;
    tcp.room = 1;
    antcore::WsFrameSender sender("", 0);
    assert(sender.send(&tcp) == 0 && tcp.addCalls == 0);
    tcp.room = 2;
    sender.send(&tcp);
    assert(tcp.writes[0] == std::vector<uint8_t>({0x81, 0x00}));
    sender.ack(2);
    assert(sender.finished());
  }
}
