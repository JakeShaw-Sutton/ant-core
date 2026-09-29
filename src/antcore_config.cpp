#include "antcore_config.h"

#include <cstring>

#include "antcore_board.h"
#include "antcore_controls.h"

namespace {

void copyString(char* dest, size_t len, const char* value) {
  if (len == 0) return;
  if (value == nullptr) value = "";
  strncpy(dest, value, len - 1);
  dest[len - 1] = '\0';
}

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

bool shouldUpdateSecret(JsonVariantConst value) {
  if (!value.is<const char*>()) return false;
  const char* text = value.as<const char*>();
  if (text == nullptr || strlen(text) == 0) return false;
  return strcmp(text, MASKED_SECRET) != 0;
}

void addValidationMessage(JsonArray array, const String& text) {
  array.add(text);
}

}  // namespace

namespace antcore_config {

bool isDefaultAdminPin(const AppConfig& config) {
  return strcmp(config.security.adminPin, DEFAULT_ADMIN_PIN) == 0;
}

void resetDefaultConfig(AppConfig& config) {
  config = AppConfig();
  for (uint8_t i = 0; i < AXIS_COUNT; i++) {
    config.control.calibration.axes[i] = AxisCalibration();
    if (i >= 4) {
      config.control.calibration.axes[i].minValue = 0.0f;
      config.control.calibration.axes[i].centerValue = 0.0f;
      config.control.calibration.axes[i].maxValue = 1.0f;
    }
  }
  config.servos[0].buttons[0].enabled = true;
  copyString(config.servos[0].buttons[0].button, sizeof(config.servos[0].buttons[0].button), "a");
  config.servos[0].buttons[0].us = 1000;
  config.servos[0].buttons[1].enabled = true;
  copyString(config.servos[0].buttons[1].button, sizeof(config.servos[0].buttons[1].button), "y");
  config.servos[0].buttons[1].us = 2000;

  copyString(config.servos[1].axis, sizeof(config.servos[1].axis), "rightX");
  config.servos[1].axisEnabled = false;
  config.servos[1].buttons[0].enabled = true;
  copyString(config.servos[1].buttons[0].button, sizeof(config.servos[1].buttons[0].button), "leftBumper");
  config.servos[1].buttons[0].us = 1000;
  config.servos[1].buttons[1].enabled = true;
  copyString(config.servos[1].buttons[1].button, sizeof(config.servos[1].buttons[1].button), "rightBumper");
  config.servos[1].buttons[1].us = 2000;
}

void configToJson(const AppConfig& config, JsonDocument& doc, bool includeSecrets) {
  doc.clear();
  doc["schema"] = config.schema;
  doc["robotName"] = config.robotName;
  doc["activeProfile"] = config.activeProfile;
  doc["apPassword"] = includeSecrets ? config.apPassword : "";
  doc["apPasswordSet"] = strlen(config.apPassword) >= 8;

  JsonObject garage = doc.createNestedObject("garage");
  garage["botType"] = config.garage.botType;
  garage["weaponType"] = config.garage.weaponType;
  garage["accent"] = config.garage.accent;
  garage["avatar"] = config.garage.avatar;
  garage["notes"] = config.garage.notes;

  JsonObject security = doc.createNestedObject("security");
  security["authEnabled"] = config.security.authEnabled;
  security["adminPin"] = includeSecrets ? config.security.adminPin : "";
  security["adminPinSet"] = strlen(config.security.adminPin) > 0;
  security["defaultPin"] = isDefaultAdminPin(config);

  JsonObject safety = doc.createNestedObject("safety");
  safety["pitMode"] = config.safety.pitMode;
  safety["mappingTestMode"] = config.control.calibration.mappingTestMode;
  safety["requireControlSource"] = config.safety.requireControlSource;

  JsonObject control = doc.createNestedObject("control");
  control["xboxArmEnabled"] = config.control.xboxArmEnabled;
  control["armButton"] = config.control.armButton;
  JsonObject calibration = control.createNestedObject("calibration");
  calibration["enabled"] = config.control.calibration.enabled;
  calibration["mappingTestMode"] = config.control.calibration.mappingTestMode;
  JsonArray calibrationAxes = calibration.createNestedArray("axes");
  for (uint8_t i = 0; i < AXIS_COUNT; i++) {
    JsonObject axis = calibrationAxes.createNestedObject();
    axis["name"] = AXIS_NAMES[i];
    axis["min"] = config.control.calibration.axes[i].minValue;
    axis["center"] = config.control.calibration.axes[i].centerValue;
    axis["max"] = config.control.calibration.axes[i].maxValue;
    axis["deadband"] = config.control.calibration.axes[i].deadband;
    axis["invert"] = config.control.calibration.axes[i].invert;
    axis["positiveOnly"] = axisIsPositiveOnly(i);
  }
  JsonObject selfRight = control.createNestedObject("selfRight");
  selfRight["enabled"] = config.control.selfRight.enabled;
  selfRight["target"] = config.control.selfRight.target;
  selfRight["servoUs"] = config.control.selfRight.servoUs;
  selfRight["motorPower"] = config.control.selfRight.motorPower;
  selfRight["durationMs"] = config.control.selfRight.durationMs;
  selfRight["cooldownMs"] = config.control.selfRight.cooldownMs;
  selfRight["requireWeaponArm"] = config.control.selfRight.requireWeaponArm;
  JsonArray actionSlots = control.createNestedArray("actions");
  for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
    JsonObject slot = actionSlots.createNestedObject();
    slot["index"] = i + 1;
    slot["enabled"] = config.control.actions[i].enabled;
    slot["button"] = config.control.actions[i].button;
    slot["action"] = config.control.actions[i].action;
  }

  JsonObject camera = doc.createNestedObject("camera");
  camera["frameSize"] = config.camera.frameSize;
  camera["jpegQuality"] = config.camera.jpegQuality;
  camera["brightness"] = config.camera.brightness;
  camera["contrast"] = config.camera.contrast;
  camera["saturation"] = config.camera.saturation;
  camera["hmirror"] = config.camera.hmirror;
  camera["vflip"] = config.camera.vflip;

  JsonObject wifiObj = doc.createNestedObject("wifi");
  wifiObj["staEnabled"] = config.wifi.staEnabled;
  wifiObj["staSsid"] = config.wifi.staSsid;
  wifiObj["staPassword"] = includeSecrets ? config.wifi.staPassword : "";
  wifiObj["staPasswordSet"] = strlen(config.wifi.staPassword) > 0;

  JsonObject batteryObj = doc.createNestedObject("battery");
  batteryObj["enabled"] = config.battery.enabled;
  batteryObj["benchMode"] = config.battery.benchMode;
  batteryObj["calibration"] = config.battery.calibration;
  batteryObj["warnVoltage"] = config.battery.warnVoltage;
  batteryObj["criticalVoltage"] = config.battery.criticalVoltage;
  batteryObj["derateEnabled"] = config.battery.derateEnabled;
  batteryObj["derateVoltage"] = config.battery.derateVoltage;
  batteryObj["derateScale"] = config.battery.derateScale;

  JsonObject drive = doc.createNestedObject("drive");
  drive["mode"] = config.drive.mode;
  drive["throttleAxis"] = config.drive.throttleAxis;
  drive["turnAxis"] = config.drive.turnAxis;
  drive["leftTankAxis"] = config.drive.leftTankAxis;
  drive["rightTankAxis"] = config.drive.rightTankAxis;
  drive["deadband"] = config.drive.deadband;
  drive["expo"] = config.drive.expo;
  drive["throttleScale"] = config.drive.throttleScale;
  drive["turnScale"] = config.drive.turnScale;
  drive["leftMotor"] = config.drive.leftMotor + 1;
  drive["rightMotor"] = config.drive.rightMotor + 1;
  drive["invertible"] = config.drive.invertible;
  drive["invertButton"] = config.drive.invertButton;
  drive["turboButton"] = config.drive.turboButton;
  drive["precisionButton"] = config.drive.precisionButton;
  drive["turboScale"] = config.drive.turboScale;
  drive["precisionScale"] = config.drive.precisionScale;
  drive["gyroAssist"] = config.drive.gyroAssist;
  drive["gyroGain"] = config.drive.gyroGain;
  drive["autoInvertWithImu"] = config.drive.autoInvertWithImu;
  drive["autoInvertAzThreshold"] = config.drive.autoInvertAzThreshold;

  JsonArray motors = doc.createNestedArray("motors");
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    JsonObject motor = motors.createNestedObject();
    motor["index"] = i + 1;
    motor["pinA"] = MOTOR_A_PINS[i];
    motor["pinB"] = MOTOR_B_PINS[i];
    motor["invert"] = config.motors[i].invert;
    motor["trim"] = config.motors[i].trim;
    motor["maxOutput"] = config.motors[i].maxOutput;
    motor["rampPerSecond"] = config.motors[i].rampPerSecond;
  }

  JsonObject weapon = doc.createNestedObject("weapon");
  weapon["enabled"] = config.weapon.enabled;
  weapon["motor"] = config.weapon.motor + 1;
  weapon["profile"] = config.weapon.profile;
  weapon["input"] = config.weapon.input;
  weapon["armButton"] = config.weapon.armButton;
  weapon["invert"] = config.weapon.invert;
  weapon["toggle"] = config.weapon.toggle;
  weapon["requireDedicatedArm"] = config.weapon.requireDedicatedArm;
  weapon["buttonPower"] = config.weapon.buttonPower;
  weapon["maxOutput"] = config.weapon.maxOutput;
  weapon["rampUpPerSecond"] = config.weapon.rampUpPerSecond;
  weapon["rampDownPerSecond"] = config.weapon.rampDownPerSecond;

  JsonArray servoArray = doc.createNestedArray("servos");
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    JsonObject servo = servoArray.createNestedObject();
    servo["index"] = i + 1;
    servo["pin"] = SERVO_PINS[i];
    servo["enabled"] = config.servos[i].enabled;
    servo["axisEnabled"] = config.servos[i].axisEnabled;
    servo["axis"] = config.servos[i].axis;
    servo["invert"] = config.servos[i].invert;
    servo["minUs"] = config.servos[i].minUs;
    servo["neutralUs"] = config.servos[i].neutralUs;
    servo["maxUs"] = config.servos[i].maxUs;
    servo["failsafeUs"] = config.servos[i].failsafeUs;
    servo["detachOnDisarm"] = config.servos[i].detachOnDisarm;
    JsonArray buttons = servo.createNestedArray("buttons");
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      JsonObject btn = buttons.createNestedObject();
      btn["enabled"] = config.servos[i].buttons[b].enabled;
      btn["button"] = config.servos[i].buttons[b].button;
      btn["us"] = config.servos[i].buttons[b].us;
      btn["toggle"] = config.servos[i].buttons[b].toggle;
    }
  }
}

bool sanitizeConfig(AppConfig& config) {
  bool disableLiveOutputs = false;
  config.schema = CONFIG_SCHEMA;
  if (strlen(config.robotName) == 0) copyString(config.robotName, sizeof(config.robotName), "Ant Core");
  if (strlen(config.activeProfile) == 0) copyString(config.activeProfile, sizeof(config.activeProfile), "Default");
  if (strlen(config.garage.botType) == 0) copyString(config.garage.botType, sizeof(config.garage.botType), "skid");
  if (strlen(config.garage.weaponType) == 0) {
    copyString(config.garage.weaponType, sizeof(config.garage.weaponType), "pusher");
  }
  if (strlen(config.garage.accent) == 0 || config.garage.accent[0] != '#') {
    copyString(config.garage.accent, sizeof(config.garage.accent), "#ffca4f");
  }
  if (strlen(config.garage.avatar) == 0) copyString(config.garage.avatar, sizeof(config.garage.avatar), "ant");
  if (strcmp(config.drive.mode, "tank") != 0 && strcmp(config.drive.mode, "arcade") != 0) {
    copyString(config.drive.mode, sizeof(config.drive.mode), "arcade");
  }
  if (strlen(config.drive.throttleAxis) == 0) copyString(config.drive.throttleAxis, sizeof(config.drive.throttleAxis), "leftY");
  if (strlen(config.drive.turnAxis) == 0) copyString(config.drive.turnAxis, sizeof(config.drive.turnAxis), "leftX");
  if (strlen(config.drive.leftTankAxis) == 0) copyString(config.drive.leftTankAxis, sizeof(config.drive.leftTankAxis), "leftY");
  if (strlen(config.drive.rightTankAxis) == 0) copyString(config.drive.rightTankAxis, sizeof(config.drive.rightTankAxis), "rightY");
  if (!isKnownAxisName(config.drive.throttleAxis)) copyString(config.drive.throttleAxis, sizeof(config.drive.throttleAxis), "leftY");
  if (!isKnownAxisName(config.drive.turnAxis)) copyString(config.drive.turnAxis, sizeof(config.drive.turnAxis), "leftX");
  if (!isKnownAxisName(config.drive.leftTankAxis)) copyString(config.drive.leftTankAxis, sizeof(config.drive.leftTankAxis), "leftY");
  if (!isKnownAxisName(config.drive.rightTankAxis)) copyString(config.drive.rightTankAxis, sizeof(config.drive.rightTankAxis), "rightY");
  if (!isKnownButtonName(config.drive.invertButton)) copyString(config.drive.invertButton, sizeof(config.drive.invertButton), "view");
  if (!isKnownButtonName(config.drive.turboButton)) copyString(config.drive.turboButton, sizeof(config.drive.turboButton), "rightStickButton");
  if (!isKnownButtonName(config.drive.precisionButton)) copyString(config.drive.precisionButton, sizeof(config.drive.precisionButton), "leftStickButton");
  config.drive.deadband = antcore::clampFloat(config.drive.deadband, 0.0f, 0.45f);
  config.drive.expo = antcore::clampFloat(config.drive.expo, 0.0f, 1.0f);
  config.drive.throttleScale = antcore::clampFloat(config.drive.throttleScale, 0.0f, 1.0f);
  config.drive.turnScale = antcore::clampFloat(config.drive.turnScale, 0.0f, 1.0f);
  config.drive.leftMotor = config.drive.leftMotor < MOTOR_COUNT ? config.drive.leftMotor : 0;
  config.drive.rightMotor = config.drive.rightMotor < MOTOR_COUNT ? config.drive.rightMotor : 1;
  if (config.drive.leftMotor == config.drive.rightMotor) config.drive.rightMotor = (config.drive.leftMotor + 1) % MOTOR_COUNT;
  config.drive.turboScale = antcore::clampFloat(config.drive.turboScale, 0.1f, 1.0f);
  config.drive.precisionScale = antcore::clampFloat(config.drive.precisionScale, 0.1f, 1.0f);
  config.drive.gyroGain = antcore::clampFloat(config.drive.gyroGain, 0.0f, 0.10f);
  config.drive.autoInvertAzThreshold = antcore::clampFloat(config.drive.autoInvertAzThreshold, -1.5f, -0.05f);
  config.battery.calibration = antcore::clampFloat(config.battery.calibration, 0.70f, 1.30f);
  if (!ANTCORE_HAS_BATTERY_SENSE) {
    // Applies to imported profiles and saved S3 configurations as well.
    config.battery.enabled = false;
    config.battery.derateEnabled = false;
  }
  config.battery.warnVoltage = antcore::clampFloat(config.battery.warnVoltage, 5.0f, 9.0f);
  config.battery.criticalVoltage = antcore::clampFloat(config.battery.criticalVoltage, 5.0f, 9.0f);
  config.battery.derateVoltage =
      antcore::clampFloat(config.battery.derateVoltage, config.battery.criticalVoltage, config.battery.warnVoltage);
  config.battery.derateScale = antcore::clampFloat(config.battery.derateScale, 0.15f, 1.0f);
  if (config.battery.criticalVoltage > config.battery.warnVoltage) {
    config.battery.criticalVoltage = config.battery.warnVoltage;
  }
  if (strlen(config.apPassword) < 8) {
    copyString(config.apPassword, sizeof(config.apPassword), DEFAULT_AP_PASSWORD);
  }
  if (config.security.authEnabled && strlen(config.security.adminPin) < 4) {
    copyString(config.security.adminPin, sizeof(config.security.adminPin), DEFAULT_ADMIN_PIN);
  }
  if (config.safety.pitMode || config.control.calibration.mappingTestMode) {
    disableLiveOutputs = true;
  }
  if (strlen(config.control.armButton) == 0) {
    copyString(config.control.armButton, sizeof(config.control.armButton), "menu");
  }
  if (!isKnownButtonName(config.control.armButton)) {
    copyString(config.control.armButton, sizeof(config.control.armButton), "menu");
  }
  for (uint8_t i = 0; i < AXIS_COUNT; i++) {
    sanitizeAxisCalibration(config.control.calibration.axes[i], i);
  }
  if (strcmp(config.control.selfRight.target, "servo1") != 0 &&
      strcmp(config.control.selfRight.target, "servo2") != 0 &&
      strcmp(config.control.selfRight.target, "weapon") != 0) {
    copyString(config.control.selfRight.target, sizeof(config.control.selfRight.target), "servo1");
  }
  config.control.selfRight.servoUs = clampInt(config.control.selfRight.servoUs, 500, 2500);
  config.control.selfRight.motorPower = antcore::clampFloat(config.control.selfRight.motorPower, -1.0f, 1.0f);
  config.control.selfRight.durationMs = clampInt(config.control.selfRight.durationMs, 50, 2000);
  config.control.selfRight.cooldownMs = clampInt(config.control.selfRight.cooldownMs, 250, 10000);
  for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
    if (!isKnownActionName(config.control.actions[i].action)) {
      copyString(config.control.actions[i].action, sizeof(config.control.actions[i].action), "none");
    }
    if (!isKnownButtonName(config.control.actions[i].button)) {
      copyString(config.control.actions[i].button, sizeof(config.control.actions[i].button), "");
    }
    if (strlen(config.control.actions[i].button) == 0) config.control.actions[i].enabled = false;
    if (!strcmp(config.control.actions[i].action, "none")) config.control.actions[i].enabled = false;
  }
  if (strcmp(config.camera.frameSize, "qqvga") != 0 && strcmp(config.camera.frameSize, "qvga") != 0 &&
      strcmp(config.camera.frameSize, "vga") != 0) {
    copyString(config.camera.frameSize, sizeof(config.camera.frameSize), "qvga");
  }
  config.camera.jpegQuality = clampInt(config.camera.jpegQuality, 10, 35);
  config.camera.brightness = clampInt(config.camera.brightness, -2, 2);
  config.camera.contrast = clampInt(config.camera.contrast, -2, 2);
  config.camera.saturation = clampInt(config.camera.saturation, -2, 2);
  if (strlen(config.wifi.staSsid) == 0) {
    copyString(config.wifi.staSsid, sizeof(config.wifi.staSsid), DEFAULT_STA_SSID);
  }
  if (strlen(config.wifi.staPassword) > 0 && strlen(config.wifi.staPassword) < 8) {
    copyString(config.wifi.staPassword, sizeof(config.wifi.staPassword), DEFAULT_STA_PASSWORD);
  }
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    config.motors[i].trim = antcore::clampFloat(config.motors[i].trim, -0.25f, 0.25f);
    config.motors[i].maxOutput = antcore::clampFloat(config.motors[i].maxOutput, 0.05f, 1.0f);
    config.motors[i].rampPerSecond = antcore::clampFloat(config.motors[i].rampPerSecond, 0.0f, 20.0f);
  }
  config.weapon.motor = config.weapon.motor < MOTOR_COUNT ? config.weapon.motor : 2;
  if (strcmp(config.weapon.profile, "spinner") != 0 && strcmp(config.weapon.profile, "lifter") != 0 &&
      strcmp(config.weapon.profile, "reversible") != 0 && strcmp(config.weapon.profile, "brushed") != 0) {
    copyString(config.weapon.profile, sizeof(config.weapon.profile), "brushed");
  }
  if (!isKnownInputName(config.weapon.input)) copyString(config.weapon.input, sizeof(config.weapon.input), "rightTrigger");
  if (!isKnownButtonName(config.weapon.armButton)) copyString(config.weapon.armButton, sizeof(config.weapon.armButton), "x");
  config.weapon.buttonPower = antcore::clampFloat(config.weapon.buttonPower, -1.0f, 1.0f);
  config.weapon.maxOutput = antcore::clampFloat(config.weapon.maxOutput, 0.0f, 1.0f);
  config.weapon.rampUpPerSecond = antcore::clampFloat(config.weapon.rampUpPerSecond, 0.1f, 20.0f);
  config.weapon.rampDownPerSecond = antcore::clampFloat(config.weapon.rampDownPerSecond, 0.1f, 30.0f);
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    config.servos[i].minUs = clampInt(config.servos[i].minUs, 500, 2400);
    config.servos[i].maxUs = clampInt(config.servos[i].maxUs, config.servos[i].minUs + 1, 2500);
    config.servos[i].neutralUs = clampInt(config.servos[i].neutralUs, config.servos[i].minUs, config.servos[i].maxUs);
    config.servos[i].failsafeUs = clampInt(config.servos[i].failsafeUs, config.servos[i].minUs, config.servos[i].maxUs);
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      if (!isKnownButtonName(config.servos[i].buttons[b].button)) {
        copyString(config.servos[i].buttons[b].button, sizeof(config.servos[i].buttons[b].button), "");
        config.servos[i].buttons[b].enabled = false;
      }
      config.servos[i].buttons[b].us =
          clampInt(config.servos[i].buttons[b].us, config.servos[i].minUs, config.servos[i].maxUs);
    }
  }
  return disableLiveOutputs;
}

bool buildConfigValidation(const AppConfig& config, bool imuPresent, JsonObject validation, String* firstError) {
  JsonArray errors = validation.createNestedArray("errors");
  JsonArray warnings = validation.createNestedArray("warnings");
  auto error = [&](const String& text) {
    if (firstError != nullptr && firstError->length() == 0) *firstError = text;
    addValidationMessage(errors, text);
  };
  auto warning = [&](const String& text) {
    addValidationMessage(warnings, text);
  };

  if (config.drive.leftMotor >= MOTOR_COUNT || config.drive.rightMotor >= MOTOR_COUNT) {
    error("drive motor index is out of range");
  }
  if (config.drive.leftMotor == config.drive.rightMotor) {
    error("left and right drive motors must be different");
  }
  if (!isKnownAxisName(config.drive.throttleAxis)) error("throttle axis is not mapped to a known controller axis");
  if (!isKnownAxisName(config.drive.turnAxis)) error("turn axis is not mapped to a known controller axis");
  if (!isKnownAxisName(config.drive.leftTankAxis)) error("tank left axis is not mapped to a known controller axis");
  if (!isKnownAxisName(config.drive.rightTankAxis)) error("tank right axis is not mapped to a known controller axis");
  if (!isKnownButtonName(config.drive.invertButton)) error("drive invert button is not mapped to a known controller button");
  if (!isKnownButtonName(config.drive.turboButton)) error("turbo button is not mapped to a known controller button");
  if (!isKnownButtonName(config.drive.precisionButton)) error("precision button is not mapped to a known controller button");
  if (config.drive.gyroAssist && !imuPresent) warning("gyro assist is enabled but the BMI270 is not detected");
  if (config.drive.autoInvertWithImu && !imuPresent) warning("auto-invert is enabled but the BMI270 is not detected");

  if (!isKnownButtonName(config.control.armButton)) error("robot arm button is not mapped to a known controller button");
  if (!config.control.calibration.enabled) warning("Xbox axis calibration is disabled");
  if (config.control.calibration.mappingTestMode) warning("controller mapping test mode blocks arming and live outputs");
  if (config.control.selfRight.enabled && config.control.selfRight.requireWeaponArm &&
      !strcmp(config.control.selfRight.target, "weapon") && !config.weapon.enabled) {
    warning("self-right weapon target is selected but weapon output is disabled");
  }
  for (uint8_t i = 0; i < ACTION_SLOT_COUNT; i++) {
    if (!config.control.actions[i].enabled) continue;
    if (!isKnownButtonName(config.control.actions[i].button)) {
      error("action slot " + String(i + 1) + " has an invalid button");
    }
    if (!isKnownActionName(config.control.actions[i].action) || !strcmp(config.control.actions[i].action, "none")) {
      error("action slot " + String(i + 1) + " has no valid action");
    }
  }

  if (config.weapon.enabled) {
    if (config.weapon.motor >= MOTOR_COUNT) {
      error("weapon motor index is out of range");
    } else if (config.weapon.motor == config.drive.leftMotor || config.weapon.motor == config.drive.rightMotor) {
      error("weapon motor overlaps a drive motor");
    }
    if (!isKnownInputName(config.weapon.input)) error("weapon input is not mapped to a known controller input");
    if (!isKnownButtonName(config.weapon.armButton)) error("weapon arm button is not mapped to a known controller button");
    if (config.weapon.requireDedicatedArm == false) warning("weapon can run without the dedicated weapon arm state");
  }

  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    if (!config.servos[i].enabled) continue;
    if (config.servos[i].minUs >= config.servos[i].maxUs) error("servo " + String(i + 1) + " min must be below max");
    if (config.servos[i].failsafeUs < config.servos[i].minUs ||
        config.servos[i].failsafeUs > config.servos[i].maxUs) {
      error("servo " + String(i + 1) + " failsafe is outside its limits");
    }
    if (config.servos[i].axisEnabled && !isKnownAxisName(config.servos[i].axis)) {
      error("servo " + String(i + 1) + " axis is not mapped to a known controller axis");
    }
    for (uint8_t b = 0; b < SERVO_BUTTON_MAPS; b++) {
      if (config.servos[i].buttons[b].enabled && !isKnownButtonName(config.servos[i].buttons[b].button)) {
        error("servo " + String(i + 1) + " button map " + String(b + 1) + " has an invalid button");
      }
    }
  }

  if (!ANTCORE_HAS_BATTERY_SENSE) warning("battery monitoring unavailable on this board; no low-voltage protection");
  else if (!config.battery.enabled) warning("battery safety is disabled");
  if (config.battery.benchMode) warning("USB bench mode blocks arming");
  if (config.safety.pitMode) warning("pit mode blocks arming and live outputs");
  if (!config.safety.requireControlSource) warning("arming can proceed without a fresh Xbox or web driver source");
  if (config.battery.criticalVoltage >= config.battery.warnVoltage) {
    warning("critical battery voltage should be below warning voltage");
  }
  if (!config.security.authEnabled) warning("admin PIN protection is disabled");
  if (config.security.authEnabled && isDefaultAdminPin(config)) warning("default admin PIN is still active");
  if (strlen(config.apPassword) < 12) warning("AP password is shorter than recommended");

  const bool valid = errors.size() == 0;
  validation["valid"] = valid;
  validation["errorCount"] = errors.size();
  validation["warningCount"] = warnings.size();
  return valid;
}

bool configIsValid(const AppConfig& config, bool imuPresent, String* firstError) {
  DynamicJsonDocument doc(CONFIG_VALIDATION_JSON_CAPACITY);
  JsonObject validation = doc.to<JsonObject>();
  return buildConfigValidation(config, imuPresent, validation, firstError);
}

bool applyConfigJson(AppConfig& config, JsonVariantConst root) {
  if (root["robotName"].is<const char*>()) {
    copyString(config.robotName, sizeof(config.robotName), root["robotName"].as<const char*>());
  }
  if (root["activeProfile"].is<const char*>()) {
    copyString(config.activeProfile, sizeof(config.activeProfile), root["activeProfile"].as<const char*>());
  }
  JsonObjectConst garageObj = root["garage"];
  if (!garageObj.isNull()) {
    if (garageObj["botType"].is<const char*>()) {
      copyString(config.garage.botType, sizeof(config.garage.botType), garageObj["botType"].as<const char*>());
    }
    if (garageObj["weaponType"].is<const char*>()) {
      copyString(config.garage.weaponType, sizeof(config.garage.weaponType),
                 garageObj["weaponType"].as<const char*>());
    }
    if (garageObj["accent"].is<const char*>()) {
      copyString(config.garage.accent, sizeof(config.garage.accent), garageObj["accent"].as<const char*>());
    }
    if (garageObj["avatar"].is<const char*>()) {
      copyString(config.garage.avatar, sizeof(config.garage.avatar), garageObj["avatar"].as<const char*>());
    }
    if (garageObj["notes"].is<const char*>()) {
      copyString(config.garage.notes, sizeof(config.garage.notes), garageObj["notes"].as<const char*>());
    }
  }
  if (shouldUpdateSecret(root["apPassword"])) {
    copyString(config.apPassword, sizeof(config.apPassword), root["apPassword"].as<const char*>());
  }
  JsonObjectConst securityObj = root["security"];
  if (!securityObj.isNull()) {
    config.security.authEnabled = securityObj["authEnabled"] | config.security.authEnabled;
    if (shouldUpdateSecret(securityObj["adminPin"])) {
      copyString(config.security.adminPin, sizeof(config.security.adminPin), securityObj["adminPin"].as<const char*>());
    }
  }
  JsonObjectConst safetyObj = root["safety"];
  if (!safetyObj.isNull()) {
    config.safety.pitMode = safetyObj["pitMode"] | config.safety.pitMode;
    config.safety.requireControlSource = safetyObj["requireControlSource"] | config.safety.requireControlSource;
  }
  JsonObjectConst controlObj = root["control"];
  if (!controlObj.isNull()) {
    JsonObjectConst calibration = controlObj["calibration"];
    if (!calibration.isNull()) {
      config.control.calibration.enabled = calibration["enabled"] | config.control.calibration.enabled;
      config.control.calibration.mappingTestMode =
          calibration["mappingTestMode"] | config.control.calibration.mappingTestMode;
      JsonArrayConst axes = calibration["axes"];
      if (!axes.isNull()) {
        for (JsonObjectConst axis : axes) {
          int8_t index = -1;
          if (axis["name"].is<const char*>()) {
            index = axisIndexByName(axis["name"].as<const char*>());
          } else if (!axis["index"].isNull()) {
            index = axis["index"].as<int>() - 1;
          }
          if (index < 0 || index >= AXIS_COUNT) continue;
          config.control.calibration.axes[index].minValue =
              axis["min"] | config.control.calibration.axes[index].minValue;
          config.control.calibration.axes[index].centerValue =
              axis["center"] | config.control.calibration.axes[index].centerValue;
          config.control.calibration.axes[index].maxValue =
              axis["max"] | config.control.calibration.axes[index].maxValue;
          config.control.calibration.axes[index].deadband =
              axis["deadband"] | config.control.calibration.axes[index].deadband;
          config.control.calibration.axes[index].invert =
              axis["invert"] | config.control.calibration.axes[index].invert;
        }
      }
    }
    JsonObjectConst selfRight = controlObj["selfRight"];
    if (!selfRight.isNull()) {
      config.control.selfRight.enabled = selfRight["enabled"] | config.control.selfRight.enabled;
      if (selfRight["target"].is<const char*>()) {
        copyString(config.control.selfRight.target, sizeof(config.control.selfRight.target),
                   selfRight["target"].as<const char*>());
      }
      config.control.selfRight.servoUs = selfRight["servoUs"] | config.control.selfRight.servoUs;
      config.control.selfRight.motorPower = selfRight["motorPower"] | config.control.selfRight.motorPower;
      config.control.selfRight.durationMs = selfRight["durationMs"] | config.control.selfRight.durationMs;
      config.control.selfRight.cooldownMs = selfRight["cooldownMs"] | config.control.selfRight.cooldownMs;
      config.control.selfRight.requireWeaponArm =
          selfRight["requireWeaponArm"] | config.control.selfRight.requireWeaponArm;
    }
    config.control.xboxArmEnabled = controlObj["xboxArmEnabled"] | config.control.xboxArmEnabled;
    if (controlObj["armButton"].is<const char*>()) {
      copyString(config.control.armButton, sizeof(config.control.armButton), controlObj["armButton"].as<const char*>());
    }
    JsonArrayConst actionSlots = controlObj["actions"];
    if (!actionSlots.isNull()) {
      for (uint8_t i = 0; i < ACTION_SLOT_COUNT && i < actionSlots.size(); i++) {
        JsonObjectConst slot = actionSlots[i];
        config.control.actions[i].enabled = slot["enabled"] | config.control.actions[i].enabled;
        if (slot["button"].is<const char*>()) {
          copyString(config.control.actions[i].button, sizeof(config.control.actions[i].button),
                     slot["button"].as<const char*>());
        }
        if (slot["action"].is<const char*>()) {
          copyString(config.control.actions[i].action, sizeof(config.control.actions[i].action),
                     slot["action"].as<const char*>());
        }
      }
    }
  }
  JsonObjectConst cameraObj = root["camera"];
  if (!cameraObj.isNull()) {
    if (cameraObj["frameSize"].is<const char*>()) {
      copyString(config.camera.frameSize, sizeof(config.camera.frameSize), cameraObj["frameSize"].as<const char*>());
    }
    config.camera.jpegQuality = cameraObj["jpegQuality"] | config.camera.jpegQuality;
    config.camera.brightness = cameraObj["brightness"] | config.camera.brightness;
    config.camera.contrast = cameraObj["contrast"] | config.camera.contrast;
    config.camera.saturation = cameraObj["saturation"] | config.camera.saturation;
    config.camera.hmirror = cameraObj["hmirror"] | config.camera.hmirror;
    config.camera.vflip = cameraObj["vflip"] | config.camera.vflip;
  }
  JsonObjectConst wifiObj = root["wifi"];
  if (!wifiObj.isNull()) {
    config.wifi.staEnabled = wifiObj["staEnabled"] | config.wifi.staEnabled;
    if (wifiObj["staSsid"].is<const char*>()) {
      copyString(config.wifi.staSsid, sizeof(config.wifi.staSsid), wifiObj["staSsid"].as<const char*>());
    }
    if (shouldUpdateSecret(wifiObj["staPassword"])) {
      copyString(config.wifi.staPassword, sizeof(config.wifi.staPassword), wifiObj["staPassword"].as<const char*>());
    }
  }
  JsonObjectConst batteryObj = root["battery"];
  if (!batteryObj.isNull()) {
    config.battery.enabled = batteryObj["enabled"] | config.battery.enabled;
    config.battery.benchMode = batteryObj["benchMode"] | config.battery.benchMode;
    config.battery.calibration = batteryObj["calibration"] | config.battery.calibration;
    config.battery.warnVoltage = batteryObj["warnVoltage"] | config.battery.warnVoltage;
    config.battery.criticalVoltage = batteryObj["criticalVoltage"] | config.battery.criticalVoltage;
    config.battery.derateEnabled = batteryObj["derateEnabled"] | config.battery.derateEnabled;
    config.battery.derateVoltage = batteryObj["derateVoltage"] | config.battery.derateVoltage;
    config.battery.derateScale = batteryObj["derateScale"] | config.battery.derateScale;
  }
  JsonObjectConst drive = root["drive"];
  if (!drive.isNull()) {
    if (drive["mode"].is<const char*>()) {
      copyString(config.drive.mode, sizeof(config.drive.mode), drive["mode"].as<const char*>());
    }
    if (drive["throttleAxis"].is<const char*>()) {
      copyString(config.drive.throttleAxis, sizeof(config.drive.throttleAxis), drive["throttleAxis"].as<const char*>());
    }
    if (drive["turnAxis"].is<const char*>()) {
      copyString(config.drive.turnAxis, sizeof(config.drive.turnAxis), drive["turnAxis"].as<const char*>());
    }
    if (drive["leftTankAxis"].is<const char*>()) {
      copyString(config.drive.leftTankAxis, sizeof(config.drive.leftTankAxis), drive["leftTankAxis"].as<const char*>());
    }
    if (drive["rightTankAxis"].is<const char*>()) {
      copyString(config.drive.rightTankAxis, sizeof(config.drive.rightTankAxis),
                 drive["rightTankAxis"].as<const char*>());
    }
    config.drive.deadband = drive["deadband"] | config.drive.deadband;
    config.drive.expo = drive["expo"] | config.drive.expo;
    config.drive.throttleScale = drive["throttleScale"] | config.drive.throttleScale;
    config.drive.turnScale = drive["turnScale"] | config.drive.turnScale;
    if (!drive["leftMotor"].isNull()) config.drive.leftMotor = max(1, drive["leftMotor"].as<int>()) - 1;
    if (!drive["rightMotor"].isNull()) config.drive.rightMotor = max(1, drive["rightMotor"].as<int>()) - 1;
    config.drive.invertible = drive["invertible"] | config.drive.invertible;
    if (drive["invertButton"].is<const char*>()) {
      copyString(config.drive.invertButton, sizeof(config.drive.invertButton), drive["invertButton"].as<const char*>());
    }
    if (drive["turboButton"].is<const char*>()) {
      copyString(config.drive.turboButton, sizeof(config.drive.turboButton), drive["turboButton"].as<const char*>());
    }
    if (drive["precisionButton"].is<const char*>()) {
      copyString(config.drive.precisionButton, sizeof(config.drive.precisionButton),
                 drive["precisionButton"].as<const char*>());
    }
    config.drive.turboScale = drive["turboScale"] | config.drive.turboScale;
    config.drive.precisionScale = drive["precisionScale"] | config.drive.precisionScale;
    config.drive.gyroAssist = drive["gyroAssist"] | config.drive.gyroAssist;
    config.drive.gyroGain = drive["gyroGain"] | config.drive.gyroGain;
    config.drive.autoInvertWithImu = drive["autoInvertWithImu"] | config.drive.autoInvertWithImu;
    config.drive.autoInvertAzThreshold = drive["autoInvertAzThreshold"] | config.drive.autoInvertAzThreshold;
  }
  JsonArrayConst motors = root["motors"];
  if (!motors.isNull()) {
    for (uint8_t i = 0; i < MOTOR_COUNT && i < motors.size(); i++) {
      JsonObjectConst motor = motors[i];
      config.motors[i].invert = motor["invert"] | config.motors[i].invert;
      config.motors[i].trim = motor["trim"] | config.motors[i].trim;
      config.motors[i].maxOutput = motor["maxOutput"] | config.motors[i].maxOutput;
      config.motors[i].rampPerSecond = motor["rampPerSecond"] | config.motors[i].rampPerSecond;
    }
  }
  JsonObjectConst weapon = root["weapon"];
  if (!weapon.isNull()) {
    config.weapon.enabled = weapon["enabled"] | config.weapon.enabled;
    if (!weapon["motor"].isNull()) config.weapon.motor = max(1, weapon["motor"].as<int>()) - 1;
    if (weapon["profile"].is<const char*>()) {
      copyString(config.weapon.profile, sizeof(config.weapon.profile), weapon["profile"].as<const char*>());
    }
    if (weapon["input"].is<const char*>()) {
      copyString(config.weapon.input, sizeof(config.weapon.input), weapon["input"].as<const char*>());
    }
    if (weapon["armButton"].is<const char*>()) {
      copyString(config.weapon.armButton, sizeof(config.weapon.armButton), weapon["armButton"].as<const char*>());
    }
    config.weapon.invert = weapon["invert"] | config.weapon.invert;
    config.weapon.toggle = weapon["toggle"] | config.weapon.toggle;
    config.weapon.requireDedicatedArm = weapon["requireDedicatedArm"] | config.weapon.requireDedicatedArm;
    config.weapon.buttonPower = weapon["buttonPower"] | config.weapon.buttonPower;
    config.weapon.maxOutput = weapon["maxOutput"] | config.weapon.maxOutput;
    config.weapon.rampUpPerSecond = weapon["rampUpPerSecond"] | config.weapon.rampUpPerSecond;
    config.weapon.rampDownPerSecond = weapon["rampDownPerSecond"] | config.weapon.rampDownPerSecond;
  }
  JsonArrayConst servoArray = root["servos"];
  if (!servoArray.isNull()) {
    for (uint8_t i = 0; i < SERVO_COUNT && i < servoArray.size(); i++) {
      JsonObjectConst servo = servoArray[i];
      config.servos[i].enabled = servo["enabled"] | config.servos[i].enabled;
      config.servos[i].axisEnabled = servo["axisEnabled"] | config.servos[i].axisEnabled;
      if (servo["axis"].is<const char*>()) {
        copyString(config.servos[i].axis, sizeof(config.servos[i].axis), servo["axis"].as<const char*>());
      }
      config.servos[i].invert = servo["invert"] | config.servos[i].invert;
      config.servos[i].minUs = servo["minUs"] | config.servos[i].minUs;
      config.servos[i].neutralUs = servo["neutralUs"] | config.servos[i].neutralUs;
      config.servos[i].maxUs = servo["maxUs"] | config.servos[i].maxUs;
      config.servos[i].failsafeUs = servo["failsafeUs"] | config.servos[i].failsafeUs;
      config.servos[i].detachOnDisarm = servo["detachOnDisarm"] | config.servos[i].detachOnDisarm;
      JsonArrayConst buttons = servo["buttons"];
      if (!buttons.isNull()) {
        for (uint8_t b = 0; b < SERVO_BUTTON_MAPS && b < buttons.size(); b++) {
          JsonObjectConst btn = buttons[b];
          config.servos[i].buttons[b].enabled = btn["enabled"] | config.servos[i].buttons[b].enabled;
          if (btn["button"].is<const char*>()) {
            copyString(config.servos[i].buttons[b].button, sizeof(config.servos[i].buttons[b].button),
                       btn["button"].as<const char*>());
          }
          config.servos[i].buttons[b].us = btn["us"] | config.servos[i].buttons[b].us;
          config.servos[i].buttons[b].toggle = btn["toggle"] | config.servos[i].buttons[b].toggle;
        }
      }
    }
  }
  return sanitizeConfig(config);
}

}  // namespace antcore_config
