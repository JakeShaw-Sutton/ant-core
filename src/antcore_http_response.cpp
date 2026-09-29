#include "antcore_http_response.h"
#include <algorithm>
#include <cstring>

namespace antcore {

namespace {
bool canQueueHttpData() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // A TCP payload also needs an MSS-sized pbuf and a Wi-Fi TX copy. Keep
  // enough RAM for incoming ACK packets; exhausting it stalls every client.
  return ESP.getFreeHeap() >= 16384 && ESP.getMaxAllocHeap() >= 2048;
#else
  return true;
#endif
}
}  // namespace

BoundedHttpResponse::BoundedHttpResponse(int code, const char* contentType, size_t length) {
  _code = code;
  _contentType = contentType;
  _contentLength = length;
}

void BoundedHttpResponse::_respond(AsyncWebServerRequest* request) {
  lastProgressMs_ = millis();
  lastAckMs_ = lastProgressMs_;
  addHeader("Connection", "close");
  head_ = _assembleHead(request->version());
  if (head_.length() == 0) {
    _state = RESPONSE_FAILED;
    request->client()->close();
    return;
  }
  _state = RESPONSE_HEADERS;
  _ack(request, 0, 0);
}

void BoundedHttpResponse::reportTransport(AsyncWebServerRequest* request, const char* event) {
#if defined(ARDUINO_ARCH_ESP32)
  Serial.printf("HTTP %s url=%s state=%u written=%u acked=%u sent=%u space=%u flush=%u add0=%u ackAge=%u heap=%u min=%u max=%u\n",
                event, request->url().c_str(), static_cast<unsigned>(_state),
                static_cast<unsigned>(_writtenLength), static_cast<unsigned>(_ackedLength),
                static_cast<unsigned>(_sentLength), static_cast<unsigned>(request->client()->space()),
                static_cast<unsigned>(flushPending_), static_cast<unsigned>(addFailures_),
                static_cast<unsigned>(millis() - lastAckMs_), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMinFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
#else
  (void)request;
  (void)event;
#endif
}

size_t BoundedHttpResponse::_ack(AsyncWebServerRequest* request, size_t len, uint32_t) {
  auto* client = request->client();
  if (!_sourceValid()) {
    _state = RESPONSE_FAILED;
    client->close();
    return 0;
  }
  _ackedLength += len;
  if (len) lastAckMs_ = lastProgressMs_ = millis();
#if defined(ARDUINO_ARCH_ESP32)
  if (!reportedLowHeap_ && ESP.getFreeHeap() < 8192) {
    reportedLowHeap_ = true;
    reportTransport(request, "low-heap");
  }
#endif
  // A failed initial TCP allocation has no outstanding packet/ACK timeout.
  // Bound that stall too, so it cannot retain its file/body indefinitely.
  if (millis() - lastProgressMs_ >= 5000) {
    reportTransport(request, "stalled");
    _state = RESPONSE_FAILED;
    client->close();
    return 0;
  }
  if (flushPending_) flushPending_ = !client->send();
  if (_state == RESPONSE_WAIT_ACK) {
    if (_ackedLength >= _writtenLength) {
      _state = RESPONSE_END;
      // The upstream request does not close RESPONSE_END itself. Closing can
      // destroy this response synchronously; return without touching it again.
      client->close();
    }
    return 0;
  }
  if (_state != RESPONSE_HEADERS && _state != RESPONSE_CONTENT) return 0;

  const size_t outstanding = _writtenLength > _ackedLength ? _writtenLength - _ackedLength : 0;
  if (outstanding >= kWindowBytes) return 0;
  size_t budget = std::min(client->space(), kWindowBytes - outstanding);
  size_t queued = 0;
  if (_state == RESPONSE_HEADERS && budget && canQueueHttpData()) {
    const size_t count = std::min(budget, head_.length() - headOffset_);
    queued = client->add(head_.c_str() + headOffset_, count);
    if (count && queued == 0) ++addFailures_;
    headOffset_ += queued;
    budget -= queued;
    if (headOffset_ == head_.length()) {
      head_ = String();
      _state = RESPONSE_CONTENT;
    }
  }
  if (_state == RESPONSE_CONTENT && budget && _sentLength < _contentLength && canQueueHttpData()) {
    uint8_t buffer[kWindowBytes];
    const size_t count = std::min(budget, _contentLength - _sentLength);
    const size_t available = readAt(_sentLength, buffer, count);
    if (available == 0 || available > count) {
      _state = RESPONSE_FAILED;
      client->close();
      return 0;
    }
    const size_t written = client->add(reinterpret_cast<const char*>(buffer), available);
    if (written == 0) ++addFailures_;
    _sentLength += written;
    queued += written;
  }
  _writtenLength += queued;
  if (queued) {
    lastProgressMs_ = millis();
    flushPending_ = !client->send();
  }
  if (_state == RESPONSE_CONTENT && _sentLength == _contentLength) {
    _state = _ackedLength >= _writtenLength ? RESPONSE_END : RESPONSE_WAIT_ACK;
  }
  return queued;
}

size_t OwnedJsonResponse::readAt(size_t offset, uint8_t* output, size_t length) {
  memcpy(output, body_.c_str() + offset, length);
  return length;
}

size_t BoundedFileResponse::readAt(size_t offset, uint8_t* output, size_t length) {
  if (!file_.seek(offset)) return 0;
  return file_.read(output, length);
}

}  // namespace antcore
