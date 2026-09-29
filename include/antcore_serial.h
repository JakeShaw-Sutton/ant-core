#pragma once

#include <Arduino.h>

enum class SerialCommandType {
  None,
  Ping,
  Help,
  Status,
  Tasks,
  Config,
  Arm,
  Disarm,
  WeaponArm,
  WeaponDisarm,
  PitMode,
  BleScan,
  BleForget,
  WifiReconnect,
  ResetConfig,
  LiveOutputEnable,
  MotorTest,
  ServoTest,
  Unknown,
};

struct ParsedSerialCommand {
  SerialCommandType type = SerialCommandType::None;
  bool boolValue = false;
  int index = 0;
  float power = 0.0f;
  int servoUs = 1500;
  int durationMs = 0;
  String error;
};

const char* serialHelpText();
ParsedSerialCommand parseSerialCommand(const String& line);
