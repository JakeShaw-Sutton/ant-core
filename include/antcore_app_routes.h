#pragma once

namespace antcore_app {

// Registration groups only; command execution belongs to the loop service.
void registerAuthRoutes();
void registerConfigurationRoutes();
void registerProfilesRoutes();
void registerControlRoutes();
void registerLogsRoutes();
void registerOutputTestsRoutes();
void registerOtaRoutes();

}  // namespace antcore_app
