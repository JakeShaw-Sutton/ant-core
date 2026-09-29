#include "app/config_state.h"
#include "app/web_state.h"
#include "app/ws_state.h"
#include "app/inputs_state.h"
#include "antcore_app_ws.h"
#include "antcore_app_commands.h"
#include "antcore_app_config.h"
#include "antcore_app_inputs.h"
#include "antcore_app_log.h"
#include "antcore_app_telemetry.h"
#include "antcore_app_web.h"
#include "antcore_logic.h"
#include "antcore_ws_message.h"
#include "antcore_http_response.h"

namespace antcore_app {

using antcore::applyDeadbandExpo;
using antcore::clampFloat;

WsState wsState;

namespace {
struct PendingClient {
  uint32_t id = 0;
  uint8_t messages = 0;
  bool telemetry = false;
};
PendingClient pendingClients[DEFAULT_MAX_WS_CLIENTS]{};
portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;
std::atomic<size_t> lastStatusBytes{8192};
std::atomic_flag statusAdmission = ATOMIC_FLAG_INIT;

bool canCopyStatus() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  // Asset/blackbox downloads and full snapshots share the C3's small network
  // budget. Let bulk files finish, then resume fresh snapshots. Existing WS
  // frames, incoming control and small replies continue; the safety loop is
  // independent of this telemetry scheduling decision.
  if (antcore::BoundedFileResponse::activeCount() != 0) return false;
  // Retain room for TCP pbufs and concurrent small HTTP/auth responses. One
  // slow peer may retain an older snapshot, but cannot retain a growing queue.
  const size_t statusBytes = lastStatusBytes.load();
  return antcore::WsPayload::allocatedBytes().load() + statusBytes <= 16384 &&
         ESP.getFreeHeap() >= statusBytes + 20480 &&
         ESP.getMaxAllocHeap() >= statusBytes + 64;
#else
  return true;
#endif
}

bool reserveMessage(uint32_t id, bool telemetry) {
  bool reserved = false;
  portENTER_CRITICAL(&pendingMux);
  PendingClient* slot = nullptr;
  for (auto& item : pendingClients) {
    if (item.id == id) { slot = &item; break; }
    if (item.id == 0 && slot == nullptr) slot = &item;
  }
  if (slot && slot->messages < 2 && (!telemetry || !slot->telemetry)) {
    slot->id = id;
    slot->messages++;
    if (telemetry) slot->telemetry = true;
    reserved = true;
  }
  portEXIT_CRITICAL(&pendingMux);
  return reserved;
}

void releaseMessage(uint32_t id, bool telemetry) {
  portENTER_CRITICAL(&pendingMux);
  for (auto& item : pendingClients) {
    if (item.id != id) continue;
    if (item.messages) item.messages--;
    if (telemetry) item.telemetry = false;
    if (item.messages == 0) item.id = 0;
    break;
  }
  portEXIT_CRITICAL(&pendingMux);
}

bool clientReady(AsyncWebSocketClient* client, bool telemetry) {
  if (client == nullptr || client->status() != WS_CONNECTED || client->queueIsFull()) return false;
  bool ready = true;
  portENTER_CRITICAL(&pendingMux);
  for (const auto& item : pendingClients) {
    if (item.id == client->id()) {
      ready = item.messages < 2 && (!telemetry || !item.telemetry);
      break;
    }
  }
  portEXIT_CRITICAL(&pendingMux);
  return ready;
}

void queueSharedText(AsyncWebSocketClient* client, antcore::WsPayload* payload, bool telemetry) {
  if (!clientReady(client, telemetry) || !reserveMessage(client->id(), telemetry)) return;
  auto* message = new (std::nothrow) antcore::WsMessage(payload, client->id(), telemetry, releaseMessage);
  if (!message) {
    releaseMessage(client->id(), telemetry);
    return;
  }
  client->message(message);
}

void sendWebSocketText(AsyncWebSocketClient* client, String text, bool telemetry = false) {
  if (text.length() == 0 || !clientReady(client, telemetry)) return;
  auto* payload = new (std::nothrow) antcore::WsPayload(std::move(text));
  if (!payload) return;
  queueSharedText(client, payload, telemetry);
  payload->release();
}
}  // namespace

void broadcastWebSocketText(String text, bool telemetry) {
  if (text.length() == 0) return;
  antcore::WsPayload* payload = nullptr;
  for (auto* client : wsState.ws.getClients()) {
    if (!clientReady(client, telemetry)) continue;
    if (!payload) payload = new (std::nothrow) antcore::WsPayload(std::move(text));
    if (!payload) return;
    queueSharedText(client, payload, telemetry);
  }
  if (payload) payload->release();
}

static bool requireWsAuth(JsonDocument& doc, const char* action);
static bool claimWebDriver(AsyncWebSocketClient* client, JsonDocument& doc, uint32_t now);
static void sendWsControlClaim(AsyncWebSocketClient* client, bool ok, const char* reason = "");
static void handleWsMessage(AsyncWebSocketClient* client, const char* payload, size_t len);
static void onWsEvent(AsyncWebSocket* socket, AsyncWebSocketClient* client, AwsEventType type,
               void* arg, uint8_t* data, size_t len);

static bool requireWsAuth(JsonDocument& doc, const char* action) {
  if (antcore_auth::tokenFromJsonIsValid(configState.cfg, webState.authState, doc)) return true;
  antcore_auth::recordFailure(webState.authState);
  addLog("WARN", String("websocket ") + action + " rejected: auth");
  return false;
}

static bool claimWebDriver(AsyncWebSocketClient* client, JsonDocument& doc, uint32_t now) {
  const char* id = doc["clientId"] | "";
  return claimWebDriverId(client->id(), id, now);
}

static void sendWsControlClaim(AsyncWebSocketClient* client, bool ok, const char* reason) {
  if (client == nullptr) return;
  ControlState snapshot;
  char driverId[WEB_DRIVER_ID_SIZE] = "";
  uint32_t driverLastMs = 0;
  getWebControlSnapshot(snapshot, driverId, sizeof(driverId), driverLastMs);
  DynamicJsonDocument reply(192);
  reply["type"] = "controlClaim";
  reply["ok"] = ok;
  reply["webDriverId"] = driverId;
  reply["reason"] = reason;
  String out;
  serializeJson(reply, out);
  sendWebSocketText(client, std::move(out));
}

static void handleWsMessage(AsyncWebSocketClient* client, const char* payload, size_t len) {
  DynamicJsonDocument doc(WS_CONTROL_JSON_CAPACITY);
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) return;
  const char* type = doc["type"] | "";
  const uint32_t now = millis();
  if (!strcmp(type, "claimControl")) {
    if (!requireWsAuth(doc, "claimControl")) {
      sendWsControlClaim(client, false, "auth required");
      return;
    }
    if (claimWebDriver(client, doc, now)) {
      addLog("INFO", "web driver lock claimed");
      sendWsControlClaim(client, true);
    } else {
      addLog("WARN", "web control claim rejected: driver locked");
      sendWsControlClaim(client, false, "driver locked");
    }
    return;
  }
  if (!strcmp(type, "releaseControl")) {
    if (!requireWsAuth(doc, "releaseControl")) return;
    if (releaseWebDriverId(client->id())) {
      addLog("INFO", "web driver lock released");
    }
    return;
  }
  if (!strcmp(type, "control")) {
    if (!requireWsAuth(doc, "control")) {
      return;
    }
    ControlState next;
    // Every packet is a full state; omitted inputs must return to neutral.
    next.leftX = clampFloat(doc["leftX"] | next.leftX, -1.0f, 1.0f);
    next.leftY = clampFloat(doc["leftY"] | next.leftY, -1.0f, 1.0f);
    next.rightX = clampFloat(doc["rightX"] | next.rightX, -1.0f, 1.0f);
    next.rightY = clampFloat(doc["rightY"] | next.rightY, -1.0f, 1.0f);
    next.leftTrigger = clampFloat(doc["leftTrigger"] | next.leftTrigger, 0.0f, 1.0f);
    next.rightTrigger = clampFloat(doc["rightTrigger"] | next.rightTrigger, 0.0f, 1.0f);
    JsonObject buttons = doc["buttons"];
    if (!buttons.isNull()) {
      next.a = buttons["a"] | false;
      next.b = buttons["b"] | false;
      next.x = buttons["x"] | false;
      next.y = buttons["y"] | false;
      next.leftBumper = buttons["leftBumper"] | false;
      next.rightBumper = buttons["rightBumper"] | false;
      next.leftStickButton = buttons["leftStickButton"] | false;
      next.rightStickButton = buttons["rightStickButton"] | false;
      next.dpadUp = buttons["dpadUp"] | false;
      next.dpadDown = buttons["dpadDown"] | false;
      next.dpadLeft = buttons["dpadLeft"] | false;
      next.dpadRight = buttons["dpadRight"] | false;
      next.menu = buttons["menu"] | false;
      next.view = buttons["view"] | false;
      next.share = buttons["share"] | false;
      next.xbox = buttons["xbox"] | false;
    }
    next.lastMs = now;
    if (!publishWebControlFrame(client->id(), doc["clientId"] | "", next, now)) return;
    if (doc["disarm"] | false) {
      RuntimeCommand command;
      command.type = RuntimeCommandType::Disarm;
      command.source = "web pad";
      submitRuntimeCommand(command);
    }
  } else if (!strcmp(type, "arm")) {
    if (!requireWsAuth(doc, "arm")) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::Arm;
    command.source = "websocket";
    submitRuntimeCommand(command);
  } else if (!strcmp(type, "disarm")) {
    if (!requireWsAuth(doc, "disarm")) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::Disarm;
    command.source = "websocket";
    submitRuntimeCommand(command);
  } else if (!strcmp(type, "weaponArm")) {
    if (!requireWsAuth(doc, "weaponArm")) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::WeaponArm;
    command.source = "websocket";
    submitRuntimeCommand(command);
  } else if (!strcmp(type, "weaponDisarm")) {
    if (!requireWsAuth(doc, "weaponDisarm")) return;
    RuntimeCommand command;
    command.type = RuntimeCommandType::WeaponDisarm;
    command.source = "websocket";
    submitRuntimeCommand(command);
  }
}

static void onWsEvent(AsyncWebSocket* socket, AsyncWebSocketClient* client, AwsEventType type,
               void* arg, uint8_t* data, size_t len) {
  (void)socket;
  if (type == WS_EVT_CONNECT) {
    // Send the second (short) TCP segment of a telemetry fragment immediately.
    // Nagle would otherwise hold it until the first segment is acknowledged.
    client->client()->setNoDelay(true);
    if (wsState.wsClientCount < 255) wsState.wsClientCount++;
    addLog("INFO", "websocket client connected");
    antcore::WsStatusAdmission admission(statusAdmission);
    if (admission && canCopyStatus()) {
      String status = cachedStatusJson(false);
      if (status.length()) lastStatusBytes = status.length();
      sendWebSocketText(client, std::move(status), true);
    }
  } else if (type == WS_EVT_DISCONNECT) {
    releaseWebDriverId(client->id());
    if (wsState.wsClientCount > 0) wsState.wsClientCount--;
    addLog("INFO", "websocket client disconnected");
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      handleWsMessage(client, reinterpret_cast<const char*>(data), len);
    }
  }
}

void initWebSocket() {
  wsState.ws.onEvent(onWsEvent);
}

void broadcastStatus() {
  // A closing or slow peer must not suppress broadcasts to healthy peers.
  // Only copy the snapshot when somebody can accept a fresh status message.
  bool ready = false;
  for (auto* client : wsState.ws.getClients()) {
    if (clientReady(client, true)) { ready = true; break; }
  }
  if (!ready) return;
  antcore::WsStatusAdmission admission(statusAdmission);
  if (admission && canCopyStatus()) {
    String status = cachedStatusJson(false);
    if (status.length()) lastStatusBytes = status.length();
    broadcastWebSocketText(std::move(status), true);
  }
}

}  // namespace antcore_app
