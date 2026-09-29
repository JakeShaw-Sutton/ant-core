#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace antcore {

// POD allocation: AsyncWebServerRequest frees _tempObject on disconnect, even
// when an upload ends before the request callback. No nested allocation leaks.
struct RequestBody {
  size_t expected;
  size_t received;
  bool valid;
  char* data() { return reinterpret_cast<char*>(this + 1); }
};

inline void appendRequestBody(void*& storage, const uint8_t* data, size_t length,
                              size_t index, size_t total, size_t maximum) {
  if (total == 0 || total > maximum) return;
  if (!storage) {
    if (index != 0) return;
    auto* body = static_cast<RequestBody*>(malloc(sizeof(RequestBody) + total + 1));
    if (!body) return;
    body->expected = total;
    body->received = 0;
    body->valid = true;
    body->data()[0] = '\0';
    storage = body;
  }
  auto* body = static_cast<RequestBody*>(storage);
  if (!body->valid) return;
  if (total != body->expected || index != body->received || index > total || length > total - index) {
    body->valid = false;
    return;
  }
  if (length) memcpy(body->data() + index, data, length);
  body->received += length;
  body->data()[body->received] = '\0';
}

inline void releaseRequestBody(void*& storage) {
  free(storage);
  storage = nullptr;
}

}  // namespace antcore
