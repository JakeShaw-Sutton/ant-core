#pragma once

#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <atomic>
#include <utility>

namespace antcore {

// Keep each HTTP connection's outstanding data bounded. The upstream basic
// response repeatedly copies its whole String; the file response fills the
// complete TCP send window. Concurrent dashboard loads exhaust C3 RAM that way.
class BoundedHttpResponse : public AsyncWebServerResponse {
 public:
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  static constexpr size_t kWindowBytes = 1024;
#else
  static constexpr size_t kWindowBytes = 2048;
#endif
  void _respond(AsyncWebServerRequest* request) override;
  size_t _ack(AsyncWebServerRequest* request, size_t len, uint32_t time) override;

 protected:
  BoundedHttpResponse(int code, const char* contentType, size_t length);
  virtual size_t readAt(size_t offset, uint8_t* output, size_t length) = 0;

 private:
  String head_;
  size_t headOffset_ = 0;
  bool flushPending_ = false;
  uint32_t lastProgressMs_ = 0;
  uint32_t lastAckMs_ = 0;
  uint32_t addFailures_ = 0;
  bool reportedLowHeap_ = false;
  void reportTransport(AsyncWebServerRequest* request, const char* event);
};

class OwnedJsonResponse final : public BoundedHttpResponse {
 public:
  OwnedJsonResponse(int code, String&& body)
      : BoundedHttpResponse(code, "application/json", body.length()), body_(std::move(body)) {}
  bool _sourceValid() const override { return body_.length() == _contentLength && _contentLength > 0; }

 protected:
  size_t readAt(size_t offset, uint8_t* output, size_t length) override;

 private:
  String body_;
};

class BoundedFileResponse final : public BoundedHttpResponse {
 public:
  BoundedFileResponse(fs::File file, const char* contentType)
      : BoundedHttpResponse(200, contentType, 0), file_(std::move(file)) {
    // VFS stdio buffering otherwise consumes up to 4KB for every open asset.
    // Set this before the first read; LittleFS already has its own page cache.
    if (file_ && !file_.setBufferSize(512)) file_.close();
    if (file_) {
      _contentLength = file_.size();
      activeCounter().fetch_add(1, std::memory_order_relaxed);
      counted_ = true;
    }
  }
  ~BoundedFileResponse() override {
    file_.close();
    if (counted_) activeCounter().fetch_sub(1, std::memory_order_relaxed);
  }
  BoundedFileResponse(const BoundedFileResponse&) = delete;
  BoundedFileResponse& operator=(const BoundedFileResponse&) = delete;
  static size_t activeCount() { return activeCounter().load(std::memory_order_relaxed); }
  bool _sourceValid() const override { return bool(file_); }

 protected:
  size_t readAt(size_t offset, uint8_t* output, size_t length) override;

 private:
  fs::File file_;
  bool counted_ = false;
  static std::atomic<size_t>& activeCounter() {
    static std::atomic<size_t> count{0};
    return count;
  }
};

}  // namespace antcore
