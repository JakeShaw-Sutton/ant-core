#include "antcore_control_owner.h"

#include <cstring>

namespace antcore {

bool WebControlSnapshot::locked(uint32_t now) const {
  return claimed && now - lastClaimMs <= WEB_DRIVER_LOCK_MS;
}

bool WebControlSnapshot::fresh(uint32_t now) const {
  return locked(now) && inputSeen && now - frame.lastMs <= WEB_CONTROL_STALE_MS;
}

bool WebControlOwner::claim(uint32_t connectionId, const char* clientId, uint32_t now) {
  if (clientId == nullptr || clientId[0] == '\0' || std::strlen(clientId) >= sizeof(state_.clientId)) return false;
  if (state_.locked(now) && state_.connectionId != connectionId) return false;
  if (!state_.locked(now) || state_.connectionId != connectionId || std::strcmp(state_.clientId, clientId) != 0) {
    clear();
    state_.claimed = true;
    state_.connectionId = connectionId;
    std::strcpy(state_.clientId, clientId);
  }
  state_.lastClaimMs = now;
  return true;
}

bool WebControlOwner::publish(uint32_t connectionId, const char* clientId, const ControlState& frame, uint32_t now) {
  if (!state_.locked(now) || state_.connectionId != connectionId || clientId == nullptr ||
      std::strcmp(state_.clientId, clientId) != 0) return false;
  state_.frame = frame;
  state_.frame.lastMs = now;
  state_.inputSeen = true;
  state_.lastClaimMs = now;
  return true;
}

bool WebControlOwner::release(uint32_t connectionId) {
  if (!state_.claimed || state_.connectionId != connectionId) return false;
  clear();
  return true;
}

void WebControlOwner::clear() {
  const uint32_t generation = state_.generation + 1;
  state_ = WebControlSnapshot();
  state_.generation = generation;
}

ControlSource selectControlSource(const WebControlSnapshot& web, bool xboxFresh, uint32_t now) {
  // A claimed web controller has priority, even while awaiting its first frame.
  if (web.locked(now)) return web.fresh(now) ? ControlSource::Web : ControlSource::None;
  return xboxFresh ? ControlSource::Xbox : ControlSource::None;
}

}  // namespace antcore
