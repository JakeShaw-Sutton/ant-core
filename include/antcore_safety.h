#pragma once

#include <cstddef>
#include <cstdint>

#include "antcore_firmware_config.h"
#include "antcore_control_owner.h"

namespace antcore_safety {

enum class RobotArmDecisionCode {
  Allowed,
  ConfigInvalid,
  PitMode,
  MappingTestMode,
  OtaActive,
  BenchMode,
  BatteryCritical,
  NoControlSource,
};

struct RobotArmSafetyInputs {
  bool configValid = true;
  const char* configError = "";
  bool pitMode = false;
  bool mappingTestMode = false;
  bool requireControlSource = true;
  bool freshControlSource = false;
  bool serialSource = false;
  bool liveOutputEnabled = false;
  bool otaInProgress = false;
  bool benchMode = false;
  bool batteryCritical = false;
};

struct RobotArmDecision {
  bool allowed = false;
  RobotArmDecisionCode code = RobotArmDecisionCode::Allowed;
};

struct RuntimeSafetyInputs {
  bool armed = false;
  bool pitMode = false;
  bool mappingTestMode = false;
  bool benchMode = false;
  bool webDriverActive = false;
  bool xboxConnected = false;
  bool xboxInputSeen = false;
  bool requireControlSource = true;
  uint32_t webInputAgeMs = 0;
  uint32_t xboxInputAgeMs = 0;
  antcore::ArmedControlOwner owner;
  uint32_t webGeneration = 0;
  bool webInputSeen = false;
};

enum class WeaponArmDecisionCode {
  Allowed,
  RobotDisarmed,
  WeaponDisabled,
};

struct WeaponArmDecision {
  bool allowed = false;
  WeaponArmDecisionCode code = WeaponArmDecisionCode::Allowed;
};

RobotArmDecision evaluateRobotArmSafety(const RobotArmSafetyInputs& inputs);
const char* robotArmDecisionReason(RobotArmDecisionCode code);
void formatRobotArmDecisionReason(const RobotArmDecision& decision, const char* configError,
                                  char* buffer, size_t bufferLen);

const char* runtimeDisarmReason(const RuntimeSafetyInputs& inputs);

WeaponArmDecision evaluateWeaponArmSafety(bool robotArmed, bool weaponEnabled);
const char* weaponArmDecisionReason(WeaponArmDecisionCode code);
const char* weaponArmLogReason(WeaponArmDecisionCode code);

}  // namespace antcore_safety
