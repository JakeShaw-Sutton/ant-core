#include "antcore_file_store.h"
#include "antcore_packs.h"

#include <LittleFS.h>

#include <cctype>

#include "antcore_firmware_config.h"
#include "antcore_logic.h"

namespace antcore_packs {

namespace {

void copyString(char* dest, size_t len, const char* value) {
  if (len == 0) return;
  if (value == nullptr) value = "";
  strlcpy(dest, value, len);
}

String safeText(const char* raw, size_t maxLen) {
  String text = raw == nullptr ? "" : String(raw);
  text.trim();
  String out;
  for (uint16_t i = 0; i < text.length() && out.length() < maxLen; i++) {
    char c = text.charAt(i);
    if (c >= 32 && c <= 126 && c != '"' && c != '\\') out += c;
  }
  out.trim();
  return out;
}

bool loadStore(JsonDocument& doc) {
  doc.clear();
  if (!LittleFS.exists(PACK_STORE_PATH)) {
    doc["ok"] = true;
    doc.createNestedArray("packs");
    return true;
  }
  File file = LittleFS.open(PACK_STORE_PATH, FILE_READ);
  if (!file) return false;
  DeserializationError err = deserializeJson(doc, file);
  if (err || !doc["packs"].is<JsonArray>()) {
    doc.clear();
    doc["ok"] = true;
    doc.createNestedArray("packs");
  }
  return true;
}

bool writeStore(JsonDocument& doc) {
  if (doc.overflowed()) return false;
  doc["ok"] = true;
  String body;
  if (serializeJson(doc, body) != measureJson(doc)) return false;
  return antcore_storage::writeTextAtomic(LittleFS, PACK_STORE_PATH, body);
}

void copyPack(JsonObjectConst src, JsonObject dst) {
  dst["id"] = src["id"] | "";
  dst["name"] = src["name"] | "";
  dst["chargeVoltage"] = src["chargeVoltage"] | 8.4f;
  dst["weak"] = src["weak"] | false;
  dst["cycles"] = src["cycles"] | 0;
  dst["notes"] = src["notes"] | "";
}

}  // namespace

String normalizePackId(const char* rawId, const char* rawName) {
  String source = rawId != nullptr && strlen(rawId) > 0 ? String(rawId) : String(rawName == nullptr ? "" : rawName);
  source.trim();
  String out;
  bool lastDash = false;
  for (uint16_t i = 0; i < source.length() && out.length() < 24; i++) {
    char c = source.charAt(i);
    if (isalnum(static_cast<unsigned char>(c))) {
      out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
      lastDash = false;
    } else if (!lastDash && out.length() > 0) {
      out += '-';
      lastDash = true;
    }
  }
  while (out.endsWith("-")) out.remove(out.length() - 1);
  if (out.length() == 0) out = "pack";
  return out;
}

void makePacksJson(bool fsMounted, JsonDocument& doc) {
  doc.clear();
  doc["ok"] = fsMounted;
  if (!fsMounted || !loadStore(doc)) {
    doc.clear();
    doc["ok"] = false;
    doc.createNestedArray("packs");
    return;
  }
  doc["ok"] = true;
}

bool savePack(bool fsMounted, JsonVariantConst root, String& savedId, String& reason) {
  if (!fsMounted) {
    reason = "pack storage unavailable";
    return false;
  }
  const String name = safeText(root["name"] | "", 31);
  savedId = normalizePackId(root["id"] | "", name.c_str());
  if (name.length() == 0) {
    reason = "pack name required";
    return false;
  }

  DynamicJsonDocument existing(PACK_LIST_JSON_CAPACITY);
  if (!loadStore(existing)) {
    reason = "pack store read failed";
    return false;
  }

  DynamicJsonDocument next(PACK_LIST_JSON_CAPACITY);
  next["ok"] = true;
  JsonArray out = next.createNestedArray("packs");
  bool replaced = false;
  JsonArray packs = existing["packs"].as<JsonArray>();
  for (JsonObjectConst pack : packs) {
    if (out.size() >= MAX_PACK_COUNT) break;
    const char* id = pack["id"] | "";
    if (savedId == id) {
      JsonObject dst = out.createNestedObject();
      dst["id"] = savedId;
      dst["name"] = name;
      dst["chargeVoltage"] = antcore::clampFloat(root["chargeVoltage"] | 8.4f, 5.0f, 9.2f);
      dst["weak"] = root["weak"] | false;
      dst["cycles"] = constrain(root["cycles"] | 0, 0, 999);
      dst["notes"] = safeText(root["notes"] | "", 95);
      replaced = true;
    } else {
      JsonObject dst = out.createNestedObject();
      copyPack(pack, dst);
    }
  }
  if (!replaced) {
    if (out.size() >= MAX_PACK_COUNT) {
      reason = "pack list full";
      return false;
    }
    JsonObject dst = out.createNestedObject();
    dst["id"] = savedId;
    dst["name"] = name;
    dst["chargeVoltage"] = antcore::clampFloat(root["chargeVoltage"] | 8.4f, 5.0f, 9.2f);
    dst["weak"] = root["weak"] | false;
    dst["cycles"] = constrain(root["cycles"] | 0, 0, 999);
    dst["notes"] = safeText(root["notes"] | "", 95);
  }

  if (!writeStore(next)) {
    reason = "pack store write failed";
    return false;
  }
  reason = replaced ? "pack updated" : "pack saved";
  return true;
}

bool deletePack(bool fsMounted, const String& requestedId, String& deletedId, String& reason) {
  if (!fsMounted) {
    reason = "pack storage unavailable";
    return false;
  }
  deletedId = normalizePackId(requestedId.c_str(), requestedId.c_str());
  DynamicJsonDocument existing(PACK_LIST_JSON_CAPACITY);
  if (!loadStore(existing)) {
    reason = "pack store read failed";
    return false;
  }
  DynamicJsonDocument next(PACK_LIST_JSON_CAPACITY);
  next["ok"] = true;
  JsonArray out = next.createNestedArray("packs");
  bool removed = false;
  for (JsonObjectConst pack : existing["packs"].as<JsonArray>()) {
    const char* id = pack["id"] | "";
    if (deletedId == id) {
      removed = true;
      continue;
    }
    JsonObject dst = out.createNestedObject();
    copyPack(pack, dst);
  }
  if (!removed) {
    reason = "pack not found";
    return false;
  }
  if (!writeStore(next)) {
    reason = "pack store write failed";
    return false;
  }
  reason = "pack deleted";
  return true;
}

}  // namespace antcore_packs
