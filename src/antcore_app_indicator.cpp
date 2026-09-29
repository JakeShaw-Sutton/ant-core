#include "app/commands_state.h"
#include "app/config_state.h"
#include "app/indicator_state.h"
#include "app/lifecycle_state.h"
#include "app/network_state.h"
#include "app/robot_state.h"
#include "app/web_state.h"
#include "antcore_app_indicator.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_lifecycle.h"
#include "antcore_app_network.h"
#include "antcore_app_robot.h"
#include "antcore_app_web.h"
#include "antcore_logic.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

IndicatorState indicatorState;

static void writeStatusPin(int pin, bool activeLow, bool on);
static void setStatusLed(bool on);
static void setAuxStatusLed(bool on);
static const char* statusPatternName(StatusPattern pattern);
static StatusPattern chooseStatusPattern();
static bool blinkPhase(uint32_t now, uint32_t periodMs, uint32_t onMs);
static bool doublePulsePhase(uint32_t now);
static bool triplePulsePhase(uint32_t now);
static void applyStatusPattern(StatusPattern pattern, uint32_t now);

static void writeStatusPin(int pin, bool activeLow, bool on) {
  digitalWrite(pin, activeLow ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

static void setStatusLed(bool on) {
#if defined(LED_BUILTIN)
  if (indicatorState.statusLedAvailable) {
    indicatorState.statusLedOn = on;
    writeStatusPin(LED_BUILTIN, STATUS_LED_ACTIVE_LOW, on);
  }
#else
  (void)on;
#endif
}

static void setAuxStatusLed(bool on) {
  if (!indicatorState.auxStatusLedAvailable) return;
  indicatorState.auxStatusLedOn = on;
  writeStatusPin(STATUS_AUX_LED_PIN, STATUS_AUX_LED_ACTIVE_LOW, on);
}

static const char* statusPatternName(StatusPattern pattern) {
  switch (pattern) {
    case StatusPattern::Booting:
      return "booting";
    case StatusPattern::WifiConnecting:
      return "wifi_connecting";
    case StatusPattern::WifiConnected:
      return "wifi_connected";
    case StatusPattern::ApFallback:
      return "ap_fallback";
    case StatusPattern::SafeBoot:
      return "safe_boot";
    case StatusPattern::Armed:
      return "armed";
    case StatusPattern::Ota:
      return "ota";
    default:
      return "unknown";
  }
}

static StatusPattern chooseStatusPattern() {
  if (commandsState.otaInProgress || commandsState.restartPending) return StatusPattern::Ota;
  if (lifecycleState.optionalPeripheralsSkipped) return StatusPattern::SafeBoot;
  if (robotState.armed || robotState.weaponArmed) return StatusPattern::Armed;
  if (!webState.webServerReady) return StatusPattern::Booting;
  if (configState.cfg.wifi.staEnabled && strlen(configState.cfg.wifi.staSsid) > 0) {
    if (staConnected()) return StatusPattern::WifiConnected;
    if (!networkState.staConnectTimeoutLogged || !networkState.staEverConnected) return StatusPattern::WifiConnecting;
    return StatusPattern::ApFallback;
  }
  return StatusPattern::ApFallback;
}

static bool blinkPhase(uint32_t now, uint32_t periodMs, uint32_t onMs) {
  return (now % periodMs) < onMs;
}

static bool doublePulsePhase(uint32_t now) {
  const uint32_t phase = now % 1200;
  return phase < 110 || (phase >= 220 && phase < 330);
}

static bool triplePulsePhase(uint32_t now) {
  const uint32_t phase = now % 1600;
  return phase < 110 || (phase >= 220 && phase < 330) || (phase >= 440 && phase < 550);
}

static void applyStatusPattern(StatusPattern pattern, uint32_t now) {
  bool mainOn = false;
  bool auxOn = false;

  switch (pattern) {
    case StatusPattern::Booting:
      mainOn = blinkPhase(now, 160, 80);        // rapid flash: firmware is alive but not ready yet.
      auxOn = false;
      break;
    case StatusPattern::WifiConnecting:
      mainOn = blinkPhase(now, 240, 120);       // fast flash: trying to join configured Wi-Fi.
      auxOn = mainOn;                           // optional second LED mirrors Wi-Fi search.
      break;
    case StatusPattern::WifiConnected:
      mainOn = true;                            // solid orange: LAN connection is up.
      auxOn = false;
      break;
    case StatusPattern::ApFallback:
      mainOn = doublePulsePhase(now);           // double pulse: AP-only/fallback mode.
      auxOn = false;
      break;
    case StatusPattern::SafeBoot:
      mainOn = triplePulsePhase(now);           // triple pulse: optional peripheral failure/brownout recovery.
      auxOn = blinkPhase(now + 350, 700, 350);
      break;
    case StatusPattern::Armed:
      mainOn = blinkPhase(now, 120, 60);        // very fast flash: outputs may be live.
      auxOn = mainOn;
      break;
    case StatusPattern::Ota:
      mainOn = blinkPhase(now, 80, 40);         // fastest flash: do not remove power.
      auxOn = !mainOn;
      break;
  }

  setStatusLed(mainOn);
  setAuxStatusLed(auxOn);
}

void initStatusLed() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  indicatorState.statusLedAvailable = true;
  setStatusLed(false);
#endif
  if (STATUS_AUX_LED_PIN >= 0) {
    pinMode(STATUS_AUX_LED_PIN, OUTPUT);
    indicatorState.auxStatusLedAvailable = true;
    setAuxStatusLed(false);
  }
}

void serviceStatusLed(uint32_t now) {
  if (!indicatorState.statusLedAvailable && !indicatorState.auxStatusLedAvailable) return;
  const StatusPattern pattern = chooseStatusPattern();
  indicatorState.statusIndicator = statusPatternName(pattern);
  applyStatusPattern(pattern, now);
}

}  // namespace antcore_app
