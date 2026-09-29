#include "antcore_safety.h"

#include <cstdio>
#include <cstring>

namespace antcore_safety {

namespace {

RobotArmDecision robotArmDecision(bool allowed, RobotArmDecisionCode code) {
  RobotArmDecision decision;
  decision.allowed = allowed;
  decision.code = code;
  return decision;
}

WeaponArmDecision weaponArmDecision(bool allowed, WeaponArmDecisionCode code) {
  WeaponArmDecision decision;
  decision.allowed = allowed;
  decision.code = code;
  return decision;
}

}  // namespace

RobotArmDecision evaluateRobotArmSafety(const RobotArmSafetyInputs& inputs) {
  if (!inputs.configValid) {
    return robotArmDecision(false, RobotArmDecisionCode::ConfigInvalid);
  }
  if (inputs.pitMode) {
    return robotArmDecision(false, RobotArmDecisionCode::PitMode);
  }
  if (inputs.mappingTestMode) {
    return robotArmDecision(false, RobotArmDecisionCode::MappingTestMode);
  }

  const bool controlSourceReady = inputs.requireControlSource ? inputs.freshControlSource : true;
  const antcore::ArmBlockReason block = antcore::evaluateArmBlock(
      inputs.otaInProgress, inputs.benchMode, inputs.batteryCritical, controlSourceReady,
      inputs.serialSource, inputs.liveOutputEnabled);

  switch (block) {
    case antcore::ArmBlockReason::OtaActive:
      return robotArmDecision(false, RobotArmDecisionCode::OtaActive);
    case antcore::ArmBlockReason::BenchMode:
      return robotArmDecision(false, RobotArmDecisionCode::BenchMode);
    case antcore::ArmBlockReason::BatteryCritical:
      return robotArmDecision(false, RobotArmDecisionCode::BatteryCritical);
    case antcore::ArmBlockReason::NoControlSource:
      return robotArmDecision(false, RobotArmDecisionCode::NoControlSource);
    case antcore::ArmBlockReason::None:
    default:
      return robotArmDecision(true, RobotArmDecisionCode::Allowed);
  }
}

const char* robotArmDecisionReason(RobotArmDecisionCode code) {
  switch (code) {
    case RobotArmDecisionCode::Allowed:
      return "";
    case RobotArmDecisionCode::ConfigInvalid:
      return "config invalid";
    case RobotArmDecisionCode::PitMode:
      return "pit mode active";
    case RobotArmDecisionCode::MappingTestMode:
      return "controller mapping test mode active";
    case RobotArmDecisionCode::OtaActive:
      return "OTA in progress";
    case RobotArmDecisionCode::BenchMode:
      return "bench mode active";
    case RobotArmDecisionCode::BatteryCritical:
      return "battery critical";
    case RobotArmDecisionCode::NoControlSource:
      return "no active control source";
    default:
      return "safety interlock";
  }
}

void formatRobotArmDecisionReason(const RobotArmDecision& decision, const char* configError,
                                  char* buffer, size_t bufferLen) {
  if (bufferLen == 0) return;
  buffer[0] = '\0';
  if (decision.code == RobotArmDecisionCode::ConfigInvalid) {
    if (configError != nullptr && strlen(configError) > 0) {
      snprintf(buffer, bufferLen, "config invalid: %s", configError);
    } else {
      snprintf(buffer, bufferLen, "%s", robotArmDecisionReason(decision.code));
    }
    buffer[bufferLen - 1] = '\0';
    return;
  }
  snprintf(buffer, bufferLen, "%s", robotArmDecisionReason(decision.code));
  buffer[bufferLen - 1] = '\0';
}

const char* runtimeDisarmReason(const RuntimeSafetyInputs& inputs) {
  if (!inputs.armed) return "";
  if (inputs.pitMode) return "pit mode active";
  if (inputs.mappingTestMode) return "controller mapping test mode active";
  if (inputs.benchMode) return "bench mode active";
  switch (inputs.owner.source) {
    case antcore::ControlSource::Web:
      if (!inputs.webDriverActive || inputs.owner.webGeneration != inputs.webGeneration) return "web control link lost";
      if (!inputs.webInputSeen || inputs.webInputAgeMs > WEB_CONTROL_DISARM_MS) return "web control timeout";
      return "";
    case antcore::ControlSource::Xbox:
      if (!inputs.xboxConnected) return "Xbox disconnected";
      if (!inputs.xboxInputSeen || inputs.xboxInputAgeMs > XBOX_CONTROL_DISARM_MS) return "Xbox control timeout";
      return "";
    case antcore::ControlSource::Test:
      return "";
    case antcore::ControlSource::None:
    default:
      return inputs.requireControlSource ? "control link lost" : "";
  }
}

WeaponArmDecision evaluateWeaponArmSafety(bool robotArmed, bool weaponEnabled) {
  if (!robotArmed) return weaponArmDecision(false, WeaponArmDecisionCode::RobotDisarmed);
  if (!weaponEnabled) return weaponArmDecision(false, WeaponArmDecisionCode::WeaponDisabled);
  return weaponArmDecision(true, WeaponArmDecisionCode::Allowed);
}

const char* weaponArmDecisionReason(WeaponArmDecisionCode code) {
  switch (code) {
    case WeaponArmDecisionCode::Allowed:
      return "";
    case WeaponArmDecisionCode::RobotDisarmed:
      return "robot disarmed";
    case WeaponArmDecisionCode::WeaponDisabled:
      return "weapon disabled";
    default:
      return "weapon safety interlock";
  }
}

const char* weaponArmLogReason(WeaponArmDecisionCode code) {
  switch (code) {
    case WeaponArmDecisionCode::WeaponDisabled:
      return "disabled";
    default:
      return weaponArmDecisionReason(code);
  }
}

}  // namespace antcore_safety
