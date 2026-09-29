#include "antcore_ws_message.h"
#include <cassert>
#include <thread>

std::atomic<unsigned> completions{0};
void complete(uint32_t id, bool) { assert(id != 0); ++completions; }

int main() {
  {
    std::atomic_flag gate = ATOMIC_FLAG_INIT;
    std::atomic<unsigned> ready{0}, attempted{0}, admitted{0};
    auto race = [&] {
      ++ready;
      while (ready < 2) std::this_thread::yield();
      antcore::WsStatusAdmission admission(gate);
      ++attempted;
      if (admission) {
        ++admitted;
        while (attempted < 2) std::this_thread::yield();
      }
    };
    std::thread connect(race), broadcast(race);
    connect.join();
    broadcast.join();
    assert(admitted == 1); // only one caller may check/copy/publish a snapshot
    {
      antcore::WsStatusAdmission next(gate);
      assert(next); // the skipped attempt did not leave the gate stuck
      {
        antcore::WsStatusAdmission skipped(gate);
        assert(!skipped);
      }
      antcore::WsStatusAdmission stillHeld(gate);
      assert(!stillHeld); // a failed acquisition must not release its owner
    }
    antcore::WsStatusAdmission later(gate);
    assert(later);
  }
  assert(antcore::WsPayload::allocatedBytes() == 0);
  auto* payload = new antcore::WsPayload(String(6000, 'x'));
  auto* first = new antcore::WsMessage(payload, 1, true, complete);
  auto* second = new antcore::WsMessage(payload, 2, true, complete);
  assert(payload->references == 3);
  payload->release();
  AsyncClient tcp;
  std::thread a([&] { first->send(&tcp); });
  std::thread b([&] { first->send(&tcp); });
  a.join(); b.join();
  assert(tcp.addCalls == 1);  // loop and AsyncTCP cannot enqueue the same frame
  assert(tcp.writes[0].size() == 2052);
  assert(!first->finished());
  first->ack(1436, 0);
  first->send(&tcp);
  assert(tcp.addCalls == 1 && !first->betweenFrames());
  first->ack(616, 0);
  while (!first->finished()) {
    first->send(&tcp);
    first->ack(tcp.writes.back().size(), 0);
  }
  assert(first->finished());
  delete first;
  assert(antcore::WsPayload::allocatedBytes() == 6000);
  delete second;
  assert(antcore::WsPayload::allocatedBytes() == 0 && completions == 2);

  for (bool ackBeforeRetry : {false, true}) {
    auto* body = new antcore::WsPayload(String("deferred"));
    auto* message = new antcore::WsMessage(body, 3, false, complete);
    body->release();
    AsyncClient client;
    client.outputOk = false;
    message->send(&client);
    assert(client.addCalls == 1 && !message->finished() && !client.closed);
    if (ackBeforeRetry) {
      message->ack(client.writes[0].size(), 0);
      const auto sends = client.sendCalls;
      assert(message->betweenFrames() && message->finished());
      assert(client.sendCalls == sends); // no retry after an earlier full ACK
    } else {
      assert(!message->betweenFrames()); // a failed retry cannot admit control
      assert(client.addCalls == 1 && !client.closed);
      client.outputOk = true;
      assert(!message->betweenFrames()); // even successful output awaits ACK
      assert(client.addCalls == 1 && !message->finished());
      message->ack(client.writes[0].size(), 0);
      assert(message->finished());
    }
    delete message;
    assert(antcore::WsPayload::allocatedBytes() == 0);
  }
  {
    auto* small = new antcore::WsPayload(String("failure"));
    auto* message = new antcore::WsMessage(small, 3, false, complete);
    small->release();
    AsyncClient client;
    client.outputOk = false;
    client.partial = true;
    client.onClose = [&] {
      // Reentrant query/destruction must run after the sender mutex releases.
      assert(message->finished());
      delete message;
      message = nullptr;
    };
    message->send(&client);
    assert(client.closed && message == nullptr);
    assert(antcore::WsPayload::allocatedBytes() == 0);
  }
  assert(completions == 5);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  {
    auto* body = new antcore::WsPayload(String(5000, 'y'));
    auto* message = new antcore::WsMessage(body, 4, true, complete);
    body->release();
    AsyncClient client;
    ESP.freeHeap = 16383;
    assert(message->send(&client) == 0 && client.addCalls == 0);
    assert(message->betweenFrames() && !message->finished() && !client.closed);
    ESP.freeHeap = 32768;
    ESP.maxAllocHeap = 2303;
    assert(message->send(&client) == 0 && client.addCalls == 0);
    ESP.maxAllocHeap = 2304;
    assert(message->send(&client) == 2048 && client.addCalls == 1);
    ESP.freeHeap = 16383;
    message->ack(1436, 0);
    message->send(&client);
    assert(!message->betweenFrames() && client.addCalls == 1);
    message->ack(616, 0); // ACK handling continues even below the heap floor.
    message->send(&client);
    assert(message->betweenFrames() && !message->finished() && client.addCalls == 1);
    ESP.freeHeap = 32768;
    while (!message->finished()) {
      message->send(&client);
      message->ack(client.writes.back().size(), 0);
    }
    assert(!client.closed && client.writes.size() == 3);
    std::string decoded;
    for (const auto& frame : client.writes) {
      const size_t header = frame[1] == 126 ? 4 : 2;
      decoded.append(frame.data() + header, frame.size() - header);
    }
    assert(decoded == String(5000, 'y'));
    delete message;
    assert(antcore::WsPayload::allocatedBytes() == 0 && completions == 6);
  }
#endif
}
