#include "antcore_serial.h"

#include <cstdio>

namespace {

bool serialTruthy(const String& value) {
  return value == "1" || value.equalsIgnoreCase("true") ||
         value.equalsIgnoreCase("yes") || value.equalsIgnoreCase("on");
}

}  // namespace

const char* serialHelpText() {
  return "OK commands: PING STATUS TASKS CONFIG? ARM DISARM WEAPON_ARM WEAPON_DISARM "
         "PIT_MODE <0|1> BLE_SCAN BLE_FORGET WIFI_RECONNECT RESET_CONFIG "
         "LIVE_OUTPUT_ENABLE <0|1> MOTOR_TEST <1-3> <-1..1> <ms> "
         "SERVO_TEST <1-2> <us> <ms>";
}

ParsedSerialCommand parseSerialCommand(const String& rawLine) {
  ParsedSerialCommand parsed;
  String line = rawLine;
  line.trim();
  if (line.length() == 0) return parsed;

  String upper = line;
  upper.toUpperCase();

  if (upper == "PING") {
    parsed.type = SerialCommandType::Ping;
  } else if (upper == "HELP") {
    parsed.type = SerialCommandType::Help;
  } else if (upper == "STATUS") {
    parsed.type = SerialCommandType::Status;
  } else if (upper == "TASKS") {
    parsed.type = SerialCommandType::Tasks;
  } else if (upper == "CONFIG?") {
    parsed.type = SerialCommandType::Config;
  } else if (upper == "ARM") {
    parsed.type = SerialCommandType::Arm;
  } else if (upper == "DISARM") {
    parsed.type = SerialCommandType::Disarm;
  } else if (upper == "WEAPON_ARM") {
    parsed.type = SerialCommandType::WeaponArm;
  } else if (upper == "WEAPON_DISARM") {
    parsed.type = SerialCommandType::WeaponDisarm;
  } else if (upper.startsWith("PIT_MODE ")) {
    parsed.type = SerialCommandType::PitMode;
    parsed.boolValue = serialTruthy(line.substring(9));
  } else if (upper == "BLE_SCAN") {
    parsed.type = SerialCommandType::BleScan;
  } else if (upper == "BLE_FORGET") {
    parsed.type = SerialCommandType::BleForget;
  } else if (upper == "WIFI_RECONNECT") {
    parsed.type = SerialCommandType::WifiReconnect;
  } else if (upper == "RESET_CONFIG") {
    parsed.type = SerialCommandType::ResetConfig;
  } else if (upper.startsWith("LIVE_OUTPUT_ENABLE ")) {
    parsed.type = SerialCommandType::LiveOutputEnable;
    parsed.boolValue = serialTruthy(line.substring(19));
  } else if (upper.startsWith("MOTOR_TEST ")) {
    parsed.type = SerialCommandType::MotorTest;
    if (sscanf(line.c_str(), "%*s %d %f %d", &parsed.index, &parsed.power,
               &parsed.durationMs) != 3 ||
        parsed.index < 1 || parsed.index > 3) {
      parsed.error = "ERR usage MOTOR_TEST <1-3> <-1..1> <ms>";
    }
  } else if (upper.startsWith("SERVO_TEST ")) {
    parsed.type = SerialCommandType::ServoTest;
    if (sscanf(line.c_str(), "%*s %d %d %d", &parsed.index, &parsed.servoUs,
               &parsed.durationMs) != 3 ||
        parsed.index < 1 || parsed.index > 2) {
      parsed.error = "ERR usage SERVO_TEST <1-2> <us> <ms>";
    }
  } else {
    parsed.type = SerialCommandType::Unknown;
  }
  return parsed;
}
