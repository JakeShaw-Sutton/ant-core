#pragma once
constexpr int BMI2_OK = 0;
constexpr int BMI2_I2C_PRIM_ADDR = 0x68;
constexpr int BMI2_I2C_SEC_ADDR = 0x69;
class BMI270 {
 public:
  struct { float accelX=0, accelY=0, accelZ=0, gyroX=0, gyroY=0, gyroZ=0; } data;
  int beginI2C(int) { return -1; }
  int getSensorData() { return -1; }
};
