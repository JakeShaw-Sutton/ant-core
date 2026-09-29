#include "antcore_http_response.h"
#include <cassert>
#include <iostream>
#include <type_traits>

static void transfer(antcore::BoundedHttpResponse& response, const std::string& expected, unsigned mode) {
  AsyncWebServerRequest request;
  if (mode == 1) request.transport.maxAccept = 37;
  if (mode == 2) request.transport.capacity = 113;
  if (mode == 3) request.transport.stallAdds = true;
  if (mode == 4) request.transport.failSendOnce = true;
  response._respond(&request);
  for (unsigned step = 0; step < 100000 && !response._finished(); ++step) {
    // Repeated polls must not fill the TCP window before the peer acknowledges.
    for (unsigned poll = 0; poll < 4; ++poll) response._ack(&request, 0, 0);
    if (step == 2) request.transport.stallAdds = false;
    const size_t acked = request.transport.acknowledge(mode == 1 ? 19 : 65535);
    response._ack(&request, acked, 0);
  }
  assert(response._finished() && !response._failed());
  const auto split = request.transport.received.find("\r\n\r\n");
  assert(split != std::string::npos);
  assert(request.transport.received.substr(split + 4) == expected);
  assert(request.transport.peak <= antcore::BoundedHttpResponse::kWindowBytes);
  assert(request.transport.closed);
}

int main() {
  static_assert(!std::is_copy_constructible<antcore::BoundedFileResponse>::value, "response owns one file counter slot");
  static_assert(!std::is_copy_assignable<antcore::BoundedFileResponse>::value, "response counter cannot be copied");
  assert(antcore::BoundedFileResponse::activeCount() == 0);
  {
    auto* first = new antcore::BoundedFileResponse(fs::File("first"), "text/plain");
    AsyncWebServerResponse* second = new antcore::BoundedFileResponse(fs::File("second"), "text/plain");
    assert(antcore::BoundedFileResponse::activeCount() == 2);
    {
      antcore::BoundedFileResponse invalid(fs::File(), "text/plain");
      antcore::BoundedFileResponse noBuffer(fs::File("x", false, true), "text/plain");
      assert(!invalid._sourceValid() && !noBuffer._sourceValid());
      assert(antcore::BoundedFileResponse::activeCount() == 2);
    }
    assert(antcore::BoundedFileResponse::activeCount() == 2);
    delete first; // disconnect/cancel before completion releases its slot
    assert(antcore::BoundedFileResponse::activeCount() == 1);
    delete second; // request destruction uses the virtual base destructor
    assert(antcore::BoundedFileResponse::activeCount() == 0);
  }
  std::string large;
  for (unsigned i = 0; i < 120000; ++i) large.push_back(char('!' + i % 90));
  for (unsigned mode = 0; mode < 5; ++mode) {
    assert(antcore::BoundedFileResponse::activeCount() == 0);
    antcore::OwnedJsonResponse json(200, String(large));
    transfer(json, large, mode);
    antcore::BoundedFileResponse file(fs::File(large), "text/plain");
    assert(antcore::BoundedFileResponse::activeCount() == 1);
    transfer(file, large, mode);
  }
  assert(antcore::BoundedFileResponse::activeCount() == 0);
  antcore::BoundedFileResponse empty(fs::File(""), "text/plain");
  transfer(empty, "", 0);
  antcore::OwnedJsonResponse missing(200, String());
  assert(!missing._sourceValid());
  AsyncWebServerRequest invalidRequest;
  antcore::BoundedFileResponse invalid(fs::File(), "text/plain");
  invalid._respond(&invalidRequest);
  assert(invalid._failed() && invalidRequest.transport.closed);
  AsyncWebServerRequest brokenRequest;
  antcore::BoundedFileResponse broken(fs::File(large, true), "text/plain");
  broken._respond(&brokenRequest);
  assert(broken._failed() && brokenRequest.transport.closed);
  antcore::BoundedFileResponse noBuffer(fs::File(large, false, true), "text/plain");
  assert(!noBuffer._sourceValid());
  AsyncWebServerRequest stalledRequest;
  stalledRequest.transport.stallAdds = true;
  antcore::OwnedJsonResponse stalled(200, String(large));
  stalled._respond(&stalledRequest);
  fakeMillis += 4999;
  stalled._ack(&stalledRequest, 0, 0);
  assert(!stalled._finished());
  fakeMillis += 1;
  stalled._ack(&stalledRequest, 0, 0);
  assert(stalled._failed() && stalledRequest.transport.closed);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  assert(antcore::BoundedHttpResponse::kWindowBytes == 1024);
  AsyncWebServerRequest pressureRequest;
  antcore::OwnedJsonResponse pressure(200, String(large));
  ESP.freeHeap = 16383;
  pressure._respond(&pressureRequest);
  assert(pressureRequest.transport.addCalls == 0);
  ESP.freeHeap = 32768;
  ESP.maxAllocHeap = 2047;
  pressure._ack(&pressureRequest, 0, 0);
  assert(pressureRequest.transport.addCalls == 0);
  ESP.maxAllocHeap = 32768;
  pressure._ack(&pressureRequest, 0, 0);
  assert(pressureRequest.transport.pending > 0);
  const size_t writesBeforePressure = pressureRequest.transport.addCalls;
  ESP.freeHeap = 16383;
  pressure._ack(&pressureRequest, pressureRequest.transport.acknowledge(65535), 0);
  assert(pressureRequest.transport.addCalls == writesBeforePressure);
  ESP.freeHeap = 32768;
  for (unsigned step = 0; step < 10000 && !pressure._finished(); ++step)
    pressure._ack(&pressureRequest, pressureRequest.transport.acknowledge(65535), 0);
  assert(pressure._finished() && !pressure._failed());
  const size_t bodyStart = pressureRequest.transport.received.find("\r\n\r\n") + 4;
  assert(pressureRequest.transport.received.substr(bodyStart) == large);
#else
  assert(antcore::BoundedHttpResponse::kWindowBytes == 2048);
#endif
  std::cout << "HTTP response transport regressions passed\n";
}
