#include "antcore_ble_input.h"

ControlState controlStateFromXboxControls(const XboxControlsState& value, uint32_t nowMs) {
  ControlState state;
  state.leftX = value.leftStickX;
  state.leftY = value.leftStickY;
  state.rightX = value.rightStickX;
  state.rightY = value.rightStickY;
  state.leftTrigger = value.leftTrigger;
  state.rightTrigger = value.rightTrigger;
  state.leftStickButton = value.leftStickButton;
  state.rightStickButton = value.rightStickButton;
  state.dpadUp = value.dpadUp;
  state.dpadDown = value.dpadDown;
  state.dpadLeft = value.dpadLeft;
  state.dpadRight = value.dpadRight;
  state.a = value.buttonA;
  state.b = value.buttonB;
  state.x = value.buttonX;
  state.y = value.buttonY;
  state.leftBumper = value.leftBumper;
  state.rightBumper = value.rightBumper;
  state.share = value.shareButton;
  state.menu = value.menuButton;
  state.view = value.viewButton;
  state.xbox = value.xboxButton;
  state.lastMs = nowMs;
  return state;
}
