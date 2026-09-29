#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "antcore_control_owner.h"

#include "antcore_app_robot.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct EventCounters {
  uint32_t controlDisconnects = 0;
  uint32_t failsafes = 0;
  uint32_t weaponArms = 0;
  uint32_t disarms = 0;
};

struct RobotState {
  antcore::ArmedControlOwner armedControlOwner;
  antcore::ControlSource lastSelectedSource = antcore::ControlSource::None;
  uint32_t lastSelectedWebGeneration = 0;
  ControlState activeState;
  OutputTest outputTest;
  Servo servos[SERVO_COUNT];
  bool servoAttached[SERVO_COUNT] = {};
  int servoOutUs[SERVO_COUNT] = {1500, 1500};
  float motorOut[MOTOR_COUNT] = {};
  float motorTarget[MOTOR_COUNT] = {};
  float weaponProfileOut = 0.0f;
  uint32_t lastWeaponProfileMs = 0;
  bool weaponToggle = false;
  bool weaponInputWasPressed = false;
  bool weaponArmed = false;
  String weaponDisarmReason = "boot";
  bool driveInverted = false;
  bool autoDriveInverted = false;
  bool lastDriveInvertButton = false;
  bool lastWeaponArmButton = false;
  bool lastActionSlotPressed[ACTION_SLOT_COUNT] = {};
  bool servoButtonToggleLatched[SERVO_COUNT][SERVO_BUTTON_MAPS] = {};
  bool lastServoButtonPressed[SERVO_COUNT][SERVO_BUTTON_MAPS] = {};
  bool servoToggleSyncNeeded = true;
  bool selfRightActive = false;
  uint32_t selfRightUntilMs = 0;
  uint32_t lastSelfRightMs = 0;
  float gyroHeadingZ = 0.0f;
  float gyroTargetZ = 0.0f;
  uint32_t lastGyroAssistMs = 0;
  bool armed = false;
  uint32_t lastControlMs = 0;
  String disarmReason = "boot";
  EventCounters eventCounters;
};

extern RobotState robotState;

}  // namespace antcore_app
