#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

#include "antcore_board.h"
#include "antcore_firmware_config.h"

void setPwmPin(int pin, uint8_t channel, uint16_t duty);
void attachPwmPin(int pin, uint8_t channel);
void writeMotorRaw(uint8_t index, float power);
void stopAllMotorPins();
void initMotorOutputs();
void attachServoOutput(uint8_t index, Servo servos[], bool servoAttached[]);
void writeServoUsOutput(uint8_t index, int us, Servo servos[], bool servoAttached[],
                        int servoOutUs[]);
void disarmServoOutputs(Servo servos[], bool servoAttached[], int servoOutUs[],
                        const ServoConfig configs[]);
