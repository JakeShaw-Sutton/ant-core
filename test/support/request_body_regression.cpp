#include "antcore_request_body.h"
#include <cassert>
#include <string>

static void append(void*& state, const std::string& value, size_t index, size_t total) {
  antcore::appendRequestBody(state, reinterpret_cast<const uint8_t*>(value.data()), value.size(), index, total, 16384);
}

int main() {
  void* state = nullptr;
  append(state, "{\"a\":", 0, 7);
  append(state, "1}", 5, 7);
  auto* body = static_cast<antcore::RequestBody*>(state);
  assert(body->valid && body->received == 7 && std::string(body->data()) == "{\"a\":1}");
  antcore::releaseRequestBody(state);
  assert(state == nullptr);
  antcore::releaseRequestBody(state);

  for (int malformed = 0; malformed < 4; ++malformed) {
    append(state, "ab", 0, 5);
    if (malformed == 0) append(state, "cd", 3, 5); // gap
    if (malformed == 1) append(state, "cd", 1, 5); // overlap
    if (malformed == 2) append(state, "cd", 2, 6); // changed size
    if (malformed == 3) append(state, "cdef", 2, 5); // overrun
    body = static_cast<antcore::RequestBody*>(state);
    assert(!body->valid && body->received == 2);
    append(state, "cde", 2, 5);
    assert(!body->valid && body->received == 2);
    antcore::releaseRequestBody(state);
  }
  append(state, "", 0, 0);
  assert(!state);
  append(state, "", 0, 16385);
  assert(!state);
  const std::string maximum(16384, 'x');
  append(state, maximum, 0, maximum.size());
  body = static_cast<antcore::RequestBody*>(state);
  assert(body->valid && body->received == maximum.size() && std::string(body->data()) == maximum);
  antcore::releaseRequestBody(state);
  append(state, "part", 0, 40);
  free(state); // AsyncWebServerRequest's disconnect/destructor cleanup contract.
}
