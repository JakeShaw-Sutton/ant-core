#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "antcore_firmware_config.h"

struct AuthState {
  char sessionToken[33] = "";
  uint32_t failureCount = 0;
  uint32_t lockoutUntilMs = 0;
  uint32_t firstFailureMs = 0;
};

namespace antcore_auth {

bool isDefaultAdminPin(const AppConfig& config);
void generateSessionToken(AuthState& state);
void clearSession(AuthState& state);
bool sessionActive(const AuthState& state);
uint32_t failures(const AuthState& state);
void recordFailure(AuthState& state);
void recordSuccess(AuthState& state);
const char* token(const AuthState& state);
bool lockedOut(const AuthState& state, uint32_t nowMs);
uint32_t lockoutRemainingMs(const AuthState& state, uint32_t nowMs);
bool adminPinIsValid(const AppConfig& config, const char* candidate);
bool tokenIsValid(const AppConfig& config, const AuthState& state, const String& candidate);
String tokenFromRequest(AsyncWebServerRequest* request);
String tokenCookieHeader(const AuthState& state);
String clearTokenCookieHeader();
bool tokenFromJsonIsValid(const AppConfig& config, const AuthState& state, JsonDocument& doc);
void sendAuthFailure(AsyncWebServerRequest* request, const char* reason = "authentication required");

}  // namespace antcore_auth
