#include "antcore_sensors.h"

#include <Wire.h>

#include "antcore_logic.h"

void sampleBatteryTelemetry(int adcPin, const BatteryConfig& config,
                            BatteryTelemetry& telemetry) {
#if ANTCORE_HAS_BATTERY_SENSE
  analogSetPinAttenuation(adcPin, ADC_11db);
  const uint32_t mv = analogReadMilliVolts(adcPin);
  const float adcVolts = mv / 1000.0f;
  const float packVolts = antcore::scaleBatteryVoltage(
      adcVolts, BATTERY_DIVIDER_MULTIPLIER, config.calibration);
  if (telemetry.packVolts <= 0.01f) {
    telemetry.adcVolts = adcVolts;
    telemetry.packVolts = packVolts;
  } else {
    telemetry.adcVolts = telemetry.adcVolts * 0.8f + adcVolts * 0.2f;
    telemetry.packVolts = telemetry.packVolts * 0.8f + packVolts * 0.2f;
  }

  const antcore::BatterySafetyResult safety = antcore::evaluateBatterySafety(
      telemetry.packVolts, config.enabled, config.warnVoltage, config.criticalVoltage,
      config.derateEnabled, config.derateVoltage);
  telemetry.cellVolts = safety.cellVolts;
  telemetry.warn = safety.warn;
  telemetry.critical = safety.critical;
  telemetry.derating = safety.derating;
#else
  // D8 is connected to the PCB divider but has no ADC on C3. In particular,
  // do not repurpose it as a motor output or enable internal pulls.
  (void)adcPin;
  (void)config;
  telemetry = BatteryTelemetry();
#endif
}

bool initImuTelemetry(BMI270& imu, ImuTelemetry& telemetry, int sdaPin, int sclPin) {
  Wire.begin(sdaPin, sclPin);
  telemetry = ImuTelemetry();
  if (imu.beginI2C(BMI2_I2C_PRIM_ADDR) == BMI2_OK) {
    telemetry.present = true;
    telemetry.address = BMI2_I2C_PRIM_ADDR;
  } else if (imu.beginI2C(BMI2_I2C_SEC_ADDR) == BMI2_OK) {
    telemetry.present = true;
    telemetry.address = BMI2_I2C_SEC_ADDR;
  }
  return telemetry.present;
}

bool sampleImuTelemetry(BMI270& imu, ImuTelemetry& telemetry) {
  if (!telemetry.present) return false;
  if (imu.getSensorData() != BMI2_OK) return false;
  telemetry.ax = imu.data.accelX;
  telemetry.ay = imu.data.accelY;
  telemetry.az = imu.data.accelZ;
  telemetry.gx = imu.data.gyroX;
  telemetry.gy = imu.data.gyroY;
  telemetry.gz = imu.data.gyroZ;
  return true;
}
