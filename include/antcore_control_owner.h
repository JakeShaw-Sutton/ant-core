#pragma once

#include "antcore_firmware_config.h"

namespace antcore {

enum class ControlSource { None, Web, Xbox, Test };

struct WebControlSnapshot {
  ControlState frame;
  char clientId[17] = "";
  uint32_t connectionId = 0;
  uint32_t lastClaimMs = 0;
  uint32_t generation = 0;
  bool claimed = false;
  bool inputSeen = false;

  bool locked(uint32_t now) const;
  bool fresh(uint32_t now) const;
};

// Platform-independent ownership policy. The firmware mailbox serializes calls.
class WebControlOwner {
 public:
  bool claim(uint32_t connectionId, const char* clientId, uint32_t now);
  bool publish(uint32_t connectionId, const char* clientId, const ControlState& frame, uint32_t now);
  bool release(uint32_t connectionId);
  void clear();
  WebControlSnapshot snapshot() const { return state_; }

 private:
  WebControlSnapshot state_;
};

struct ArmedControlOwner {
  ControlSource source = ControlSource::None;
  uint32_t webGeneration = 0;
};

ControlSource selectControlSource(const WebControlSnapshot& web, bool xboxFresh, uint32_t now);

}  // namespace antcore
