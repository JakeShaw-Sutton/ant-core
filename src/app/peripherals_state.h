#pragma once

#include "antcore_sensors.h"
#include "camera_stream.h"

#include "antcore_app_peripherals.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct PeripheralsState {
  BMI270 imu;
  BatteryTelemetry battery;
  ImuTelemetry imuTelemetry;
  CameraStreamStats cameraStats;
  bool cameraReady = false;
  String lastCameraError = ANTCORE_HAS_CAMERA ? "" : "Camera disabled on XIAO ESP32-C3";
  bool cameraInitAttempted = false;
};

extern PeripheralsState peripheralsState;

}  // namespace antcore_app
