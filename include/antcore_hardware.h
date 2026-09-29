#pragma once

#if defined(ARDUINO)
#include <sdkconfig.h>
#endif

// Select capabilities from the compiled chip target; never from saved profiles.
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define ANTCORE_HAS_CAMERA 0
#define ANTCORE_HAS_BATTERY_SENSE 0
#define ANTCORE_SHARED_MOTOR_PWM 1
constexpr const char* BOARD_NAME = "XIAO ESP32-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || !defined(ARDUINO)
#define ANTCORE_HAS_CAMERA 1
#define ANTCORE_HAS_BATTERY_SENSE 1
#define ANTCORE_SHARED_MOTOR_PWM 0
constexpr const char* BOARD_NAME = "XIAO ESP32-S3 Sense";
#else
#error "Ant Core supports XIAO ESP32-S3 Sense and XIAO ESP32-C3 only"
#endif

// The existing PCB divider is unchanged. C3 D8 is left undriven and unread.
constexpr float BATTERY_DIVIDER_MULTIPLIER = 3.0f;
