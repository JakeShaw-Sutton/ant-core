#pragma once

#include <cstddef>
#include <cstdint>

#include "antcore_logic.h"
#include "antcore_hardware.h"

#if __has_include("antcore_local_secrets.h")
#include "antcore_local_secrets.h"
#endif

#ifndef ANTCORE_DEFAULT_AP_PASSWORD
#define ANTCORE_DEFAULT_AP_PASSWORD "antcore123"
#endif
#ifndef ANTCORE_DEFAULT_STA_ENABLED
#define ANTCORE_DEFAULT_STA_ENABLED false
#endif
#ifndef ANTCORE_DEFAULT_STA_SSID
#define ANTCORE_DEFAULT_STA_SSID ""
#endif
#ifndef ANTCORE_DEFAULT_STA_PASSWORD
#define ANTCORE_DEFAULT_STA_PASSWORD ""
#endif
#ifndef ANTCORE_HEADLESS_STA_DEFAULT
#define ANTCORE_HEADLESS_STA_DEFAULT ANTCORE_DEFAULT_STA_ENABLED
#endif
#ifndef ANTCORE_DEFAULT_ADMIN_PIN
#define ANTCORE_DEFAULT_ADMIN_PIN "antcore"
#endif
#ifndef ANTCORE_STATUS_LED_ACTIVE_LOW
#define ANTCORE_STATUS_LED_ACTIVE_LOW true
#endif
#ifndef ANTCORE_STATUS_AUX_LED_PIN
#define ANTCORE_STATUS_AUX_LED_PIN -1
#endif
#ifndef ANTCORE_STATUS_AUX_LED_ACTIVE_LOW
#define ANTCORE_STATUS_AUX_LED_ACTIVE_LOW true
#endif

using antcore::AxisCalibration;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t CONFIG_SCHEMA = 11;
constexpr const char* FIRMWARE_VERSION = "1.2.1-shared-pcb";
constexpr const char* WEB_ASSET_VERSION = "2026-09-16-shared-pcb";

constexpr uint8_t MOTOR_COUNT = 3;
constexpr uint8_t SERVO_COUNT = 2;
constexpr uint8_t SERVO_BUTTON_MAPS = 4;
constexpr uint8_t ACTION_SLOT_COUNT = 4;

constexpr uint32_t MOTOR_PWM_FREQ = 20000;
constexpr uint8_t MOTOR_PWM_BITS = 10;
constexpr uint16_t MOTOR_PWM_MAX = (1U << MOTOR_PWM_BITS) - 1U;

constexpr uint32_t STATUS_BROADCAST_MS = 200;
constexpr uint32_t SENSOR_SAMPLE_MS = 50;
constexpr uint32_t WEB_CONTROL_STALE_MS = 250;
constexpr uint32_t WEB_CONTROL_DISARM_MS = 750;
constexpr uint32_t XBOX_CONTROL_STALE_MS = 1000;
constexpr uint32_t XBOX_CONTROL_DISARM_MS = 1000;
constexpr uint32_t WEB_DRIVER_LOCK_MS = 2000;
constexpr uint32_t OTA_RESTART_DELAY_MS = 900;
constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t STA_RECONNECT_MS = 15000;
constexpr uint16_t CAPTIVE_DNS_PORT = 53;
constexpr uint8_t LOG_RING_SIZE = 64;
constexpr uint8_t RUNTIME_COMMAND_QUEUE_SIZE = 8;
constexpr uint8_t PENDING_LOG_QUEUE_SIZE = 16;
constexpr uint8_t BLACKBOX_WRITE_QUEUE_SIZE = 16;
constexpr uint8_t STATUS_LOG_MAX_LINES = ANTCORE_SHARED_MOTOR_PWM ? 8 : 16;
constexpr uint8_t API_LOG_MAX_LINES = 32;
constexpr uint32_t BLACKBOX_SERVICE_INTERVAL_MS = 50;
constexpr size_t LOG_LINE_MAX_LEN = 192;
constexpr size_t CONFIG_JSON_CAPACITY = ANTCORE_SHARED_MOTOR_PWM ? 8192 : 16384;
constexpr size_t STATUS_JSON_CAPACITY = ANTCORE_SHARED_MOTOR_PWM ? 12288 : 20480;
constexpr size_t LOG_JSON_CAPACITY = 8192;
constexpr size_t WS_CONTROL_JSON_CAPACITY = 1536;
constexpr size_t PROFILE_LIST_JSON_CAPACITY = 4096;
constexpr size_t CONFIG_VALIDATION_JSON_CAPACITY = 3072;
constexpr size_t PACK_LIST_JSON_CAPACITY = 4096;

constexpr const char* PREF_NAMESPACE = "antcore";
constexpr const char* PREF_CONFIG_KEY = "config";
constexpr const char* CONFIG_FILE_PATH = "/config.json";
constexpr const char* PROFILE_DIR = "/profiles";
constexpr const char* PACK_STORE_PATH = "/packs.json";
constexpr const char* BLACKBOX_LOG_PATH = "/blackbox.log";
constexpr size_t BLACKBOX_LOG_MAX_BYTES = 32768;
constexpr size_t BLACKBOX_LOG_KEEP_BYTES = 16384;
constexpr const char* AUTH_COOKIE_NAME = "AntCoreToken";
constexpr uint32_t AUTH_LOCKOUT_MS = 15000;
constexpr uint8_t AUTH_LOCKOUT_FAILURES = 5;
constexpr uint32_t AUTH_FAILURE_WINDOW_MS = 60000;
constexpr const char* DEFAULT_AP_PASSWORD = ANTCORE_DEFAULT_AP_PASSWORD;
constexpr const char* DEFAULT_STA_SSID = ANTCORE_DEFAULT_STA_SSID;
constexpr const char* DEFAULT_STA_PASSWORD = ANTCORE_DEFAULT_STA_PASSWORD;
constexpr bool HEADLESS_STA_DEFAULT = ANTCORE_HEADLESS_STA_DEFAULT;
constexpr const char* DEFAULT_ADMIN_PIN = ANTCORE_DEFAULT_ADMIN_PIN;
constexpr const char* MASKED_SECRET = "********";
constexpr const char* MDNS_HOSTNAME = "antcore";
constexpr bool STATUS_LED_ACTIVE_LOW = ANTCORE_STATUS_LED_ACTIVE_LOW;
constexpr int STATUS_AUX_LED_PIN = ANTCORE_STATUS_AUX_LED_PIN;
constexpr bool STATUS_AUX_LED_ACTIVE_LOW = ANTCORE_STATUS_AUX_LED_ACTIVE_LOW;
constexpr uint8_t MAX_PROFILE_COUNT = 8;
constexpr uint8_t MAX_PACK_COUNT = 10;
constexpr uint8_t AXIS_COUNT = 6;
constexpr const char* AXIS_NAMES[] = {"leftX", "leftY", "rightX", "rightY", "leftTrigger", "rightTrigger"};
constexpr const char* BUTTON_NAMES[] = {"a", "b", "x", "y", "leftBumper", "rightBumper", "leftStickButton",
                                        "rightStickButton", "dpadUp", "dpadDown", "dpadLeft", "dpadRight",
                                        "menu", "view", "share", "xbox"};
constexpr const char* ACTION_NAMES[] = {"none", "robotArmToggle", "robotDisarm", "weaponArmToggle",
                                        "weaponSafe", "driveInvertToggle", "selfRight"};

struct MotorConfig {
  bool invert = false;
  float trim = 0.0f;
  float maxOutput = 1.0f;
  float rampPerSecond = 6.0f;
};

struct WeaponConfig {
  bool enabled = false;
  uint8_t motor = 2;
  char profile[16] = "brushed";
  char input[20] = "rightTrigger";
  char armButton[20] = "x";
  bool invert = false;
  bool toggle = false;
  bool requireDedicatedArm = true;
  float buttonPower = 1.0f;
  float maxOutput = 1.0f;
  float rampUpPerSecond = 3.0f;
  float rampDownPerSecond = 8.0f;
};

struct ServoButtonMap {
  bool enabled = false;
  char button[20] = "";
  int us = 1500;
  bool toggle = false;
};

struct ServoConfig {
  bool enabled = true;
  bool axisEnabled = true;
  char axis[20] = "rightY";
  bool invert = false;
  int minUs = 1000;
  int neutralUs = 1500;
  int maxUs = 2000;
  int failsafeUs = 1500;
  bool detachOnDisarm = false;
  ServoButtonMap buttons[SERVO_BUTTON_MAPS];
};

struct BatteryConfig {
  bool enabled = ANTCORE_HAS_BATTERY_SENSE;
  bool benchMode = false;
  float calibration = 1.0f;
  float warnVoltage = 7.0f;
  float criticalVoltage = 6.4f;
  bool derateEnabled = false;
  float derateVoltage = 6.8f;
  float derateScale = 0.70f;
};

struct WifiConfig {
  bool staEnabled = ANTCORE_DEFAULT_STA_ENABLED;
  char staSsid[33] = ANTCORE_DEFAULT_STA_SSID;
  char staPassword[64] = ANTCORE_DEFAULT_STA_PASSWORD;
};

struct DriveConfig {
  char mode[16] = "arcade";
  char throttleAxis[20] = "leftY";
  char turnAxis[20] = "leftX";
  char leftTankAxis[20] = "leftY";
  char rightTankAxis[20] = "rightY";
  float deadband = 0.07f;
  float expo = 0.25f;
  float throttleScale = 1.0f;
  float turnScale = 1.0f;
  uint8_t leftMotor = 0;
  uint8_t rightMotor = 1;
  bool invertible = true;
  char invertButton[20] = "view";
  char turboButton[20] = "rightStickButton";
  char precisionButton[20] = "leftStickButton";
  float turboScale = 1.0f;
  float precisionScale = 0.45f;
  bool gyroAssist = false;
  float gyroGain = 0.015f;
  bool autoInvertWithImu = false;
  float autoInvertAzThreshold = -0.45f;
};

struct ControlConfig {
  struct CalibrationConfig {
    bool enabled = true;
    bool mappingTestMode = false;
    AxisCalibration axes[AXIS_COUNT];
  } calibration;
  bool xboxArmEnabled = true;
  char armButton[20] = "menu";
  struct SelfRightConfig {
    bool enabled = false;
    char target[12] = "servo1";
    int servoUs = 2000;
    float motorPower = 1.0f;
    uint16_t durationMs = 550;
    uint16_t cooldownMs = 1500;
    bool requireWeaponArm = true;
  } selfRight;
  struct ActionSlot {
    bool enabled = false;
    char button[20] = "";
    char action[24] = "none";
  } actions[ACTION_SLOT_COUNT];
};

struct CameraConfig {
  char frameSize[12] = "qvga";
  int jpegQuality = 18;
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  bool hmirror = false;
  bool vflip = false;
};

struct SecurityConfig {
  bool authEnabled = true;
  char adminPin[20] = "antcore";
};

struct SafetyConfig {
  bool pitMode = false;
  bool requireControlSource = true;
};

struct GarageConfig {
  char botType[20] = "skid";
  char weaponType[24] = "pusher";
  char accent[8] = "#ffca4f";
  char avatar[16] = "ant";
  char notes[160] = "";
};

struct AppConfig {
  uint32_t schema = CONFIG_SCHEMA;
  char robotName[32] = "Ant Core";
  char apPassword[64] = "antcore123";
  char activeProfile[24] = "Default";
  GarageConfig garage;
  SecurityConfig security;
  SafetyConfig safety;
  ControlConfig control;
  CameraConfig camera;
  WifiConfig wifi;
  BatteryConfig battery;
  DriveConfig drive;
  MotorConfig motors[MOTOR_COUNT];
  WeaponConfig weapon;
  ServoConfig servos[SERVO_COUNT];
};

struct ControlState {
  float leftX = 0.0f;
  float leftY = 0.0f;
  float rightX = 0.0f;
  float rightY = 0.0f;
  float leftTrigger = 0.0f;
  float rightTrigger = 0.0f;
  bool leftStickButton = false;
  bool rightStickButton = false;
  bool dpadUp = false;
  bool dpadDown = false;
  bool dpadLeft = false;
  bool dpadRight = false;
  bool a = false;
  bool b = false;
  bool x = false;
  bool y = false;
  bool leftBumper = false;
  bool rightBumper = false;
  bool share = false;
  bool menu = false;
  bool view = false;
  bool xbox = false;
  uint32_t lastMs = 0;
};

struct BatteryTelemetry {
  float adcVolts = 0.0f;
  float packVolts = 0.0f;
  float cellVolts = 0.0f;
  bool warn = false;
  bool critical = false;
  bool derating = false;
};

struct ImuTelemetry {
  bool present = false;
  uint8_t address = 0;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
};

struct OutputTest {
  bool liveOutputEnabled = false;
  bool motorActive = false;
  uint8_t motor = 0;
  float motorPower = 0.0f;
  bool servoActive = false;
  uint8_t servo = 0;
  int servoUs = 1500;
  uint32_t motorUntilMs = 0;
  uint32_t servoUntilMs = 0;
};
