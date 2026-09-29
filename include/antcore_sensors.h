#pragma once

#include <Arduino.h>
#include <SparkFun_BMI270_Arduino_Library.h>

#include "antcore_firmware_config.h"

void sampleBatteryTelemetry(int adcPin, const BatteryConfig& config,
                            BatteryTelemetry& telemetry);
bool initImuTelemetry(BMI270& imu, ImuTelemetry& telemetry, int sdaPin, int sclPin);
bool sampleImuTelemetry(BMI270& imu, ImuTelemetry& telemetry);
