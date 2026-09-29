#pragma once

#include "antcore_app_commands.h"

// Application-internal state. Public callers use the service functions.
namespace antcore_app {

struct CommandsState {
  bool otaInProgress = false;
  volatile bool filesystemOtaInProgress = false;
  bool restartPending = false;
  uint32_t restartAtMs = 0;
  QueueHandle_t runtimeCommandQueue = nullptr;
};

extern CommandsState commandsState;

}  // namespace antcore_app
