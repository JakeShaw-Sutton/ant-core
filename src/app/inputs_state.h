#pragma once

#include <BLEGamepadClient.h>

#include "antcore_app_inputs.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct InputsState {
  XboxController xbox;
  ControlState xboxState;
  ControlState xboxRawState;
  antcore::WebControlOwner webControlOwner;
  bool lastXboxMenu = false;
  bool bleReady = false;
  portMUX_TYPE xboxInputMux = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE webControlMux = portMUX_INITIALIZER_UNLOCKED;
  XboxControlsState pendingXboxControls;
  bool pendingXboxControlsAvailable = false;
  uint32_t pendingXboxControlsMs = 0;
  volatile bool pendingXboxDisconnected = false;
};

extern InputsState inputsState;

constexpr size_t WEB_DRIVER_ID_SIZE = 17;

}  // namespace antcore_app
