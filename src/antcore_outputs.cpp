#include "antcore_outputs.h"

#include <cmath>

#include "antcore_logic.h"

using antcore::clampFloat;

#if ANTCORE_SHARED_MOTOR_PWM
namespace {
int activeMotorPins[MOTOR_COUNT] = {-1, -1, -1};

void detachMotorPwm(int pin, uint8_t channel) {
  setPwmPin(pin, channel, 0);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(pin);
#else
  ledcDetachPin(pin);
#endif
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}
}  // namespace
#endif

void initMotorOutputs() {
#if ANTCORE_SHARED_MOTOR_PWM
  // ESP32Servo must never allocate a motor channel or change its timer frequency.
  ESP32PWM::allocateTimer(SERVO_PWM_TIMER);
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (activeMotorPins[i] >= 0) detachMotorPwm(activeMotorPins[i], MOTOR_PWM_CHANNELS[i][0]);
    activeMotorPins[i] = -1;
    digitalWrite(MOTOR_A_PINS[i], LOW);
    digitalWrite(MOTOR_B_PINS[i], LOW);
    pinMode(MOTOR_A_PINS[i], OUTPUT);
    pinMode(MOTOR_B_PINS[i], OUTPUT);
  }
#else
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    attachPwmPin(MOTOR_A_PINS[i], MOTOR_PWM_CHANNELS[i][0]);
    attachPwmPin(MOTOR_B_PINS[i], MOTOR_PWM_CHANNELS[i][1]);
  }
#endif
}

void setPwmPin(int pin, uint8_t channel, uint16_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(channel, duty);
#endif
}

void attachPwmPin(int pin, uint8_t channel) {
  pinMode(pin, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(pin, MOTOR_PWM_FREQ, MOTOR_PWM_BITS, channel);
#else
  ledcSetup(channel, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
  ledcAttachPin(pin, channel);
#endif
  setPwmPin(pin, channel, 0);
}

void writeMotorRaw(uint8_t index, float power) {
  if (index >= MOTOR_COUNT) return;
  power = clampFloat(power, -1.0f, 1.0f);
  const int pinA = MOTOR_A_PINS[index];
  const int pinB = MOTOR_B_PINS[index];
  const uint8_t chA = MOTOR_PWM_CHANNELS[index][0];
  const uint8_t chB = MOTOR_PWM_CHANNELS[index][1];
#if ANTCORE_SHARED_MOTOR_PWM
  (void)chB;
  const int nextPin = fabsf(power) < 0.002f ? -1 : (power > 0.0f ? pinA : pinB);
  if (activeMotorPins[index] != nextPin) {
    // Break before make: disconnect the old PWM and force it low before
    // routing the channel to the opposite bridge input at zero duty.
    if (activeMotorPins[index] >= 0) detachMotorPwm(activeMotorPins[index], chA);
    activeMotorPins[index] = nextPin;
    if (nextPin >= 0) attachPwmPin(nextPin, chA);
  }
  if (nextPin >= 0) setPwmPin(nextPin, chA, static_cast<uint16_t>(fabsf(power) * MOTOR_PWM_MAX));
  return;
#else
  if (fabsf(power) < 0.002f) {
    setPwmPin(pinA, chA, 0);
    setPwmPin(pinB, chB, 0);
    return;
  }
  uint16_t duty = static_cast<uint16_t>(fabsf(power) * MOTOR_PWM_MAX);
  if (power > 0.0f) {
    setPwmPin(pinA, chA, duty);
    setPwmPin(pinB, chB, 0);
  } else {
    setPwmPin(pinA, chA, 0);
    setPwmPin(pinB, chB, duty);
  }
#endif
}

void stopAllMotorPins() {
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    writeMotorRaw(i, 0.0f);
  }
}

void attachServoOutput(uint8_t index, Servo servos[], bool servoAttached[]) {
  if (index >= SERVO_COUNT || servoAttached[index]) return;
  servos[index].setPeriodHertz(50);
  servos[index].attach(SERVO_PINS[index], 500, 2500);
  servoAttached[index] = true;
}

void writeServoUsOutput(uint8_t index, int us, Servo servos[], bool servoAttached[],
                        int servoOutUs[]) {
  if (index >= SERVO_COUNT) return;
  attachServoOutput(index, servos, servoAttached);
  servos[index].writeMicroseconds(us);
  servoOutUs[index] = us;
}

void disarmServoOutputs(Servo servos[], bool servoAttached[], int servoOutUs[],
                        const ServoConfig configs[]) {
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    if (!configs[i].enabled) {
      if (servoAttached[i]) {
        servos[i].detach();
        servoAttached[i] = false;
      }
      continue;
    }
    if (configs[i].detachOnDisarm) {
      if (servoAttached[i]) {
        servos[i].detach();
        servoAttached[i] = false;
      }
    } else {
      writeServoUsOutput(i, configs[i].failsafeUs, servos, servoAttached, servoOutUs);
    }
  }
}
