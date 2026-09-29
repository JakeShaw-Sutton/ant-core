#pragma once
#include <Arduino.h>
#include <cassert>
#include <limits>

struct HttpTestEsp {
  size_t freeHeap = 65536, maxAllocHeap = 65536;
  size_t getFreeHeap() const { return freeHeap; }
  size_t getMaxAllocHeap() const { return maxAllocHeap; }
};
inline HttpTestEsp ESP;

class AsyncClient {
 public:
  std::string received;
  size_t pending = 0, peak = 0, maxAccept = 65535, addCalls = 0;
  size_t capacity = 65535;
  bool stallAdds = false, failSendOnce = false, closed = false;
  size_t space() const { return capacity > pending ? capacity - pending : 0; }
  size_t add(const char* data, size_t length) {
    ++addCalls;
    if (stallAdds) return 0;
    size_t count = std::min(length, std::min(space(), maxAccept));
    received.append(data, count);
    pending += count;
    peak = std::max(peak, pending);
    return count;
  }
  bool send() {
    if (failSendOnce) { failSendOnce = false; return false; }
    return true;
  }
  void close() { closed = true; }
  size_t acknowledge(size_t maximum) {
    const size_t count = std::min(pending, maximum);
    pending -= count;
    return count;
  }
};
class AsyncWebServerRequest {
 public:
  AsyncClient transport;
  AsyncClient* client() { return &transport; }
  uint8_t version() const { return 1; }
};
enum WebResponseState { RESPONSE_SETUP, RESPONSE_HEADERS, RESPONSE_CONTENT, RESPONSE_WAIT_ACK, RESPONSE_END, RESPONSE_FAILED };
class AsyncWebServerResponse {
 protected:
  int _code = 0;
  String _contentType;
  size_t _contentLength = 0, _headLength = 0, _sentLength = 0, _ackedLength = 0, _writtenLength = 0;
  WebResponseState _state = RESPONSE_SETUP;
 public:
  virtual ~AsyncWebServerResponse() = default;
  virtual void _respond(AsyncWebServerRequest*) = 0;
  virtual size_t _ack(AsyncWebServerRequest*, size_t, uint32_t) = 0;
  virtual bool _sourceValid() const = 0;
  virtual void addHeader(const String&, const String&) {}
  virtual String _assembleHead(uint8_t) {
    return "HTTP/1.1 " + String(_code) + "\r\nContent-Length: " + String(_contentLength) + "\r\n\r\n";
  }
  bool _finished() const { return _state > RESPONSE_WAIT_ACK; }
  bool _failed() const { return _state == RESPONSE_FAILED; }
};
