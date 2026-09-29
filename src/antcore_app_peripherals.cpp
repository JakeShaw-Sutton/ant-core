#include "antcore_sensors.h"
#include "camera_stream.h"
#include "antcore_board.h"
#include "app/config_state.h"
#include "app/peripherals_state.h"
#include "app/robot_state.h"
#include "app/web_state.h"
#include "antcore_app_peripherals.h"
#include "antcore_app_config.h"
#include "antcore_app_log.h"
#include "antcore_app_robot.h"
#include "antcore_app_web.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

PeripheralsState peripheralsState;



void initCamera() {
  peripheralsState.cameraReady = initCameraRuntime(configState.cfg.camera, peripheralsState.cameraReady, peripheralsState.lastCameraError, peripheralsState.cameraStats,
                                  &configState.cfg.security.authEnabled, webState.authState.sessionToken);
  addLog(peripheralsState.cameraReady ? "INFO" : "WARN", peripheralsState.cameraReady ? "camera ready on :81" : peripheralsState.lastCameraError);
}

void sampleBattery() {
  sampleBatteryTelemetry(PIN_BATTERY_SENSE, configState.cfg.battery, peripheralsState.battery);
  if (robotState.armed && peripheralsState.battery.critical) {
    disarmRobot("battery critical");
  }
}

void initImu() {
  initImuTelemetry(peripheralsState.imu, peripheralsState.imuTelemetry, PIN_I2C_SDA, PIN_I2C_SCL);
  addLog(peripheralsState.imuTelemetry.present ? "INFO" : "INFO",
         peripheralsState.imuTelemetry.present ? "BMI270 detected" : "BMI270 not detected");
}

void sampleImu() {
  sampleImuTelemetry(peripheralsState.imu, peripheralsState.imuTelemetry);
}

}  // namespace antcore_app
