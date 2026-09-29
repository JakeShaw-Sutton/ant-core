#pragma once
#include <Arduino.h>

namespace fs {
class File {
 public:
  File() = default;
  explicit File(std::string data, bool failReads = false, bool failBuffer = false)
      : data_(std::move(data)), valid_(true), failReads_(failReads), failBuffer_(failBuffer) {}
  size_t size() const { return data_.size(); }
  explicit operator bool() const { return valid_; }
  bool setBufferSize(size_t size) { assert(size == 512); return !failBuffer_; }
  void close() { valid_ = false; }
  bool seek(size_t offset) { offset_ = offset; return valid_ && offset <= data_.size(); }
  size_t read(uint8_t* output, size_t length) {
    if (failReads_) return 0;
    const size_t count = std::min(length, data_.size() - offset_);
    memcpy(output, data_.data() + offset_, count);
    offset_ += count;
    return count;
  }
 private:
  std::string data_;
  size_t offset_ = 0;
  bool valid_ = false, failReads_ = false, failBuffer_ = false;
};
}
