#pragma once

#include <Arduino.h>

struct CameraConfig;

struct CameraStreamStats {
  uint32_t frames = 0;
  uint32_t snapshots = 0;
  uint32_t activeClients = 0;
  uint32_t lastFrameAtMs = 0;
  uint32_t lastClientAtMs = 0;
  uint32_t lastFrameBytes = 0;
  float fps = 0.0f;
  float avgCaptureMs = 0.0f;
  float avgSendMs = 0.0f;
};

void applyCameraSensorSettings(const CameraConfig& settings);
bool initCameraRuntime(const CameraConfig& settings, bool& cameraReady, String& cameraError,
                       CameraStreamStats& streamStats, const bool* authEnabled,
                       const char* sessionToken);
bool startCameraStreamServer(bool* cameraReadyFlag, String* cameraError, CameraStreamStats* streamStats,
                             const bool* authEnabled, const char* sessionToken);
