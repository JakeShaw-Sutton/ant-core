#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "antcore_firmware_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace antcore_app {

enum class RuntimeCommandType {
  Arm,
  Disarm,
  WeaponArm,
  WeaponDisarm,
  PitSet,
  ReleaseWebControl,
  ApplyConfig,
  ResetConfig,
  SaveProfile,
  LoadProfile,
  LiveOutputSet,
  MotorTest,
  ServoTest,
  BleForget,
  OtaBeginFirmware,
  OtaBeginFilesystem,
  OtaFinish,
};

struct RuntimeCommand {
  RuntimeCommandType type = RuntimeCommandType::Disarm;
  String source;
  String text;
  bool boolValue = false;
  int index = 0;
  int durationMs = 0;
  int servoUs = 0;
  float power = 0.0f;
  bool ok = false;
  int status = 500;
  bool armedResult = false;
  bool weaponArmedResult = false;
  bool boolResult = false;
  String name;
  String message;
  SemaphoreHandle_t done = nullptr;
};

void serviceRuntimeCommandQueue(uint8_t maxCommands = RUNTIME_COMMAND_QUEUE_SIZE);
bool submitRuntimeCommand(RuntimeCommand& command);

}  // namespace antcore_app
