#include "antcore_auth.h"

#include <cstring>

#include <esp_system.h>

#include "antcore_config.h"

namespace antcore_auth {

namespace {

bool constantTimeEquals(const char* a, const char* b, size_t expectedLen) {
  if (a == nullptr || b == nullptr) return false;
  uint8_t diff = 0;
  size_t lenA = strnlen(a, expectedLen + 1);
  size_t lenB = strnlen(b, expectedLen + 1);
  diff |= static_cast<uint8_t>(lenA ^ expectedLen);
  diff |= static_cast<uint8_t>(lenB ^ expectedLen);
  for (size_t i = 0; i < expectedLen; i++) {
    const char ca = i < lenA ? a[i] : '\0';
    const char cb = i < lenB ? b[i] : '\0';
    diff |= static_cast<uint8_t>(ca ^ cb);
  }
  return diff == 0;
}

String tokenFromCookie(const String& cookieHeader) {
  const String prefix = String(AUTH_COOKIE_NAME) + "=";
  int start = cookieHeader.indexOf(prefix);
  if (start < 0) return "";
  start += prefix.length();
  int end = cookieHeader.indexOf(';', start);
  if (end < 0) end = cookieHeader.length();
  return cookieHeader.substring(start, end);
}

}  // namespace

bool isDefaultAdminPin(const AppConfig& config) {
  return antcore_config::isDefaultAdminPin(config);
}

void generateSessionToken(AuthState& state) {
  static const char* alphabet = "0123456789abcdef";
  for (uint8_t i = 0; i < 32; i++) {
    state.sessionToken[i] = alphabet[esp_random() & 0x0F];
  }
  state.sessionToken[32] = '\0';
}

void clearSession(AuthState& state) {
  state.sessionToken[0] = '\0';
}

bool sessionActive(const AuthState& state) {
  return state.sessionToken[0] != '\0';
}

uint32_t failures(const AuthState& state) {
  return state.failureCount;
}

void recordFailure(AuthState& state) {
  const uint32_t now = millis();
  if (state.firstFailureMs == 0 || now - state.firstFailureMs > AUTH_FAILURE_WINDOW_MS) {
    state.firstFailureMs = now;
    state.failureCount = 0;
  }
  state.failureCount++;
  if (state.failureCount >= AUTH_LOCKOUT_FAILURES) {
    state.lockoutUntilMs = now + AUTH_LOCKOUT_MS;
  }
}

void recordSuccess(AuthState& state) {
  state.failureCount = 0;
  state.firstFailureMs = 0;
  state.lockoutUntilMs = 0;
}

const char* token(const AuthState& state) {
  return state.sessionToken;
}

bool lockedOut(const AuthState& state, uint32_t nowMs) {
  return state.lockoutUntilMs != 0 && static_cast<int32_t>(nowMs - state.lockoutUntilMs) < 0;
}

uint32_t lockoutRemainingMs(const AuthState& state, uint32_t nowMs) {
  if (!lockedOut(state, nowMs)) return 0;
  return state.lockoutUntilMs - nowMs;
}

bool adminPinIsValid(const AppConfig& config, const char* candidate) {
  if (!config.security.authEnabled) return true;
  const size_t expectedLen = strnlen(config.security.adminPin, sizeof(config.security.adminPin));
  return constantTimeEquals(candidate == nullptr ? "" : candidate, config.security.adminPin, expectedLen);
}

bool tokenIsValid(const AppConfig& config, const AuthState& state, const String& candidate) {
  if (!config.security.authEnabled) return true;
  return sessionActive(state) && constantTimeEquals(candidate.c_str(), state.sessionToken, 32);
}

String tokenFromRequest(AsyncWebServerRequest* request) {
  if (request->hasHeader("X-AntCore-Token")) {
    return request->getHeader("X-AntCore-Token")->value();
  }
  if (request->hasHeader("Cookie")) {
    return tokenFromCookie(request->getHeader("Cookie")->value());
  }
  return "";
}

String tokenCookieHeader(const AuthState& state) {
  return String(AUTH_COOKIE_NAME) + "=" + state.sessionToken + "; Path=/; SameSite=Strict; Max-Age=86400";
}

String clearTokenCookieHeader() {
  return String(AUTH_COOKIE_NAME) + "=; Path=/; SameSite=Strict; Max-Age=0";
}

bool tokenFromJsonIsValid(const AppConfig& config, const AuthState& state, JsonDocument& doc) {
  const char* candidate = doc["token"] | "";
  return tokenIsValid(config, state, String(candidate));
}

void sendAuthFailure(AsyncWebServerRequest* request, const char* reason) {
  DynamicJsonDocument doc(192);
  doc["ok"] = false;
  doc["error"] = "auth_required";
  doc["reason"] = reason;
  String out;
  serializeJson(doc, out);
  AsyncWebServerResponse* response = request->beginResponse(401, "application/json", out);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

}  // namespace antcore_auth
