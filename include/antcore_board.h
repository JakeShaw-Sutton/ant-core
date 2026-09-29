#pragma once

#include <Arduino.h>

#include "antcore_firmware_config.h"

constexpr int MOTOR_A_PINS[MOTOR_COUNT] = {D0, D2, D9};
constexpr int MOTOR_B_PINS[MOTOR_COUNT] = {D1, D3, D10};
constexpr int PIN_BATTERY_SENSE = D8;
#if ANTCORE_SHARED_MOTOR_PWM
// One channel follows the active input of each bridge; inactive input stays low.
constexpr uint8_t MOTOR_PWM_CHANNELS[MOTOR_COUNT][2] = {{0, 0}, {1, 1}, {2, 2}};
constexpr uint8_t SERVO_PWM_TIMER = 2;  // channels 4/5, isolated from motor timers 0/1
#else
constexpr uint8_t MOTOR_PWM_CHANNELS[MOTOR_COUNT][2] = {{0, 1}, {2, 3}, {4, 5}};
#endif
constexpr int SERVO_PINS[SERVO_COUNT] = {D6, D7};
constexpr int PIN_I2C_SDA = D4;
constexpr int PIN_I2C_SCL = D5;
#if ANTCORE_HAS_CAMERA
// XIAO ESP32-S3 Sense camera pins. SD-card support is intentionally unused on
// Ant Core because those SD pins overlap the motor and battery pins.
constexpr int PWDN_GPIO_NUM = -1;
constexpr int RESET_GPIO_NUM = -1;
constexpr int XCLK_GPIO_NUM = 10;
constexpr int SIOD_GPIO_NUM = 40;
constexpr int SIOC_GPIO_NUM = 39;
constexpr int Y9_GPIO_NUM = 48;
constexpr int Y8_GPIO_NUM = 11;
constexpr int Y7_GPIO_NUM = 12;
constexpr int Y6_GPIO_NUM = 14;
constexpr int Y5_GPIO_NUM = 16;
constexpr int Y4_GPIO_NUM = 18;
constexpr int Y3_GPIO_NUM = 17;
constexpr int Y2_GPIO_NUM = 15;
constexpr int VSYNC_GPIO_NUM = 38;
constexpr int HREF_GPIO_NUM = 47;
constexpr int PCLK_GPIO_NUM = 13;
#endif
