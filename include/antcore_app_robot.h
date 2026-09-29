#pragma once

#include <Arduino.h>
#include <cstdint>

namespace antcore_app {

void disarmRobot(const String& reason);
bool canArm(String& reason, const String& source);
void armRobot(const String& source);
void disarmWeapon(const String& reason);
void armWeapon(const String& source);
void toggleArm(const String& source);
uint16_t motorDutyForSide(float power, bool sideA);
void stopAllMotors();
void disarmServos();
bool selfRightIsActive(uint32_t now);
void updateOutputs();
void initPins();

}  // namespace antcore_app
