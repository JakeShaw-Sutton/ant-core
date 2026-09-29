#include "camera_stream.h"

#include "antcore_board.h"
#include "antcore_firmware_config.h"

#if ANTCORE_HAS_CAMERA
#include <esp_camera.h>
#include <esp_http_server.h>
#include <cstring>

namespace {

constexpr uint8_t CAMERA_XCLK_LEDC_CHANNEL = 7;
constexpr uint8_t CAMERA_XCLK_LEDC_TIMER = 3;
constexpr uint8_t CAMERA_MAX_STREAM_CLIENTS = 1;

constexpr uint8_t ledcTimerIndexForChannel(uint8_t channel) {
  return (channel / 2U) % 4U;
}

static_assert(CAMERA_XCLK_LEDC_TIMER == 3, "Camera XCLK must stay on LEDC timer 3.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[0][0]), "Camera XCLK conflicts with motor 1A PWM.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[0][1]), "Camera XCLK conflicts with motor 1B PWM.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[1][0]), "Camera XCLK conflicts with motor 2A PWM.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[1][1]), "Camera XCLK conflicts with motor 2B PWM.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[2][0]), "Camera XCLK conflicts with motor 3A PWM.");
static_assert(CAMERA_XCLK_LEDC_TIMER != ledcTimerIndexForChannel(MOTOR_PWM_CHANNELS[2][1]), "Camera XCLK conflicts with motor 3B PWM.");

httpd_handle_t cameraHttpd = nullptr;
bool* cameraReady = nullptr;
String* lastCameraError = nullptr;
CameraStreamStats* stats = nullptr;
const bool* cameraAuthEnabled = nullptr;
const char* cameraSessionToken = nullptr;

void setCameraError(const char* message) {
  if (lastCameraError) {
    *lastCameraError = message;
  }
}

bool isCameraReady() {
  return cameraReady != nullptr && *cameraReady;
}

bool constantTimeTokenEquals(const char* candidate, const char* expected) {
  if (candidate == nullptr || expected == nullptr) return false;
  uint8_t diff = 0;
  const size_t candidateLen = strnlen(candidate, 33);
  const size_t expectedLen = strnlen(expected, 33);
  diff |= static_cast<uint8_t>(candidateLen ^ 32U);
  diff |= static_cast<uint8_t>(expectedLen ^ 32U);
  for (uint8_t i = 0; i < 32; i++) {
    const char a = i < candidateLen ? candidate[i] : '\0';
    const char b = i < expectedLen ? expected[i] : '\0';
    diff |= static_cast<uint8_t>(a ^ b);
  }
  return diff == 0;
}

bool tokenFromCookieHeader(const char* cookie, char* out, size_t outLen) {
  if (cookie == nullptr || out == nullptr || outLen == 0) return false;
  out[0] = '\0';
  const char* key = AUTH_COOKIE_NAME;
  const size_t keyLen = strlen(key);
  const char* cursor = cookie;
  while (*cursor != '\0') {
    while (*cursor == ' ' || *cursor == ';') cursor++;
    if (strncmp(cursor, key, keyLen) == 0 && cursor[keyLen] == '=') {
      cursor += keyLen + 1;
      size_t i = 0;
      while (cursor[i] != '\0' && cursor[i] != ';' && i + 1 < outLen) {
        out[i] = cursor[i];
        i++;
      }
      out[i] = '\0';
      return true;
    }
    const char* next = strchr(cursor, ';');
    if (next == nullptr) break;
    cursor = next + 1;
  }
  return false;
}

bool cameraRequestAuthorized(httpd_req_t* req) {
  if (cameraAuthEnabled == nullptr || !*cameraAuthEnabled) return true;
  if (cameraSessionToken == nullptr || cameraSessionToken[0] == '\0') return false;
  char cookie[160];
  if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return false;
  char token[40];
  if (!tokenFromCookieHeader(cookie, token, sizeof(token))) return false;
  return constantTimeTokenEquals(token, cameraSessionToken);
}

bool rejectUnauthorizedCameraRequest(httpd_req_t* req) {
  if (cameraRequestAuthorized(req)) return false;
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"auth_required\",\"reason\":\"camera authentication required\"}");
  return true;
}

esp_err_t snapshotHandler(httpd_req_t* req) {
  if (rejectUnauthorizedCameraRequest(req)) return ESP_OK;
  if (!isCameraReady()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera not available");
    return ESP_FAIL;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    setCameraError("capture failed");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture failed");
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t res = httpd_resp_send(req, reinterpret_cast<const char*>(fb->buf), fb->len);
  if (stats) {
    stats->snapshots++;
    stats->lastFrameBytes = fb->len;
    stats->lastFrameAtMs = millis();
  }
  esp_camera_fb_return(fb);
  return res;
}

esp_err_t streamHandler(httpd_req_t* req) {
  if (rejectUnauthorizedCameraRequest(req)) return ESP_OK;
  if (!isCameraReady()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera not available");
    return ESP_FAIL;
  }
  if (stats && stats->activeClients >= CAMERA_MAX_STREAM_CLIENTS) {
    httpd_resp_set_status(req, "429 Too Many Requests");
    httpd_resp_sendstr(req, "camera stream already in use");
    return ESP_OK;
  }
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "X-Framerate", "30");

  if (stats) {
    stats->activeClients++;
    stats->lastClientAtMs = millis();
  }

  while (isCameraReady()) {
    const uint32_t captureStartMs = millis();
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      setCameraError("capture failed");
      if (stats && stats->activeClients > 0) {
        stats->activeClients--;
      }
      return ESP_FAIL;
    }
    const uint32_t captureMs = millis() - captureStartMs;

    char partHeader[96];
    int headerLen = snprintf(partHeader, sizeof(partHeader),
                             "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                             static_cast<unsigned int>(fb->len));
    const uint32_t sendStartMs = millis();
    esp_err_t res = httpd_resp_send_chunk(req, partHeader, headerLen);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(fb->buf), fb->len);
    }
    const uint32_t sendMs = millis() - sendStartMs;
    const uint32_t nowMs = millis();
    if (stats) {
      const uint32_t lastFrameAtMs = stats->lastFrameAtMs;
      if (lastFrameAtMs > 0 && nowMs > lastFrameAtMs) {
        const float instFps = 1000.0f / static_cast<float>(nowMs - lastFrameAtMs);
        stats->fps = stats->fps <= 0.01f ? instFps : (stats->fps * 0.85f + instFps * 0.15f);
      }
      stats->frames++;
      stats->lastFrameAtMs = nowMs;
      stats->lastClientAtMs = nowMs;
      stats->lastFrameBytes = fb->len;
      stats->avgCaptureMs = stats->avgCaptureMs <= 0.01f ? captureMs : (stats->avgCaptureMs * 0.85f + captureMs * 0.15f);
      stats->avgSendMs = stats->avgSendMs <= 0.01f ? sendMs : (stats->avgSendMs * 0.85f + sendMs * 0.15f);
    }
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  if (stats && stats->activeClients > 0) {
    stats->activeClients--;
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

}  // namespace

bool startCameraStreamServer(bool* cameraReadyFlag, String* cameraError, CameraStreamStats* streamStats,
                             const bool* authEnabled, const char* sessionToken) {
  cameraReady = cameraReadyFlag;
  lastCameraError = cameraError;
  stats = streamStats;
  cameraAuthEnabled = authEnabled;
  cameraSessionToken = sessionToken;
  if (cameraHttpd) return true;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.ctrl_port = 32769;
  config.stack_size = 8192;
  config.max_open_sockets = 3;
  if (httpd_start(&cameraHttpd, &config) != ESP_OK) {
    cameraHttpd = nullptr;
    setCameraError("camera HTTP server failed");
    return false;
  }

  httpd_uri_t streamUri = {};
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;
  streamUri.user_ctx = nullptr;
  httpd_register_uri_handler(cameraHttpd, &streamUri);

  httpd_uri_t snapshotUri = {};
  snapshotUri.uri = "/snapshot.jpg";
  snapshotUri.method = HTTP_GET;
  snapshotUri.handler = snapshotHandler;
  snapshotUri.user_ctx = nullptr;
  httpd_register_uri_handler(cameraHttpd, &snapshotUri);

  return true;
}

namespace {

framesize_t configuredFrameSize(const CameraConfig& settings) {
  if (!strcmp(settings.frameSize, "qqvga")) return FRAMESIZE_QQVGA;
  if (!strcmp(settings.frameSize, "vga")) return FRAMESIZE_VGA;
  return FRAMESIZE_QVGA;
}

}  // namespace

void applyCameraSensorSettings(const CameraConfig& settings) {
  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) return;
  sensor->set_framesize(sensor, configuredFrameSize(settings));
  sensor->set_quality(sensor, settings.jpegQuality);
  sensor->set_brightness(sensor, settings.brightness);
  sensor->set_contrast(sensor, settings.contrast);
  sensor->set_saturation(sensor, settings.saturation);
  sensor->set_hmirror(sensor, settings.hmirror ? 1 : 0);
  sensor->set_vflip(sensor, settings.vflip ? 1 : 0);
}

bool initCameraRuntime(const CameraConfig& settings, bool& cameraReadyFlag, String& cameraError,
                       CameraStreamStats& streamStats, const bool* authEnabled,
                       const char* sessionToken) {
  camera_config_t sensorConfig = {};
  sensorConfig.ledc_channel = LEDC_CHANNEL_7;
  sensorConfig.ledc_timer = LEDC_TIMER_3;
  sensorConfig.pin_d0 = Y2_GPIO_NUM;
  sensorConfig.pin_d1 = Y3_GPIO_NUM;
  sensorConfig.pin_d2 = Y4_GPIO_NUM;
  sensorConfig.pin_d3 = Y5_GPIO_NUM;
  sensorConfig.pin_d4 = Y6_GPIO_NUM;
  sensorConfig.pin_d5 = Y7_GPIO_NUM;
  sensorConfig.pin_d6 = Y8_GPIO_NUM;
  sensorConfig.pin_d7 = Y9_GPIO_NUM;
  sensorConfig.pin_xclk = XCLK_GPIO_NUM;
  sensorConfig.pin_pclk = PCLK_GPIO_NUM;
  sensorConfig.pin_vsync = VSYNC_GPIO_NUM;
  sensorConfig.pin_href = HREF_GPIO_NUM;
  sensorConfig.pin_sccb_sda = SIOD_GPIO_NUM;
  sensorConfig.pin_sccb_scl = SIOC_GPIO_NUM;
  sensorConfig.pin_pwdn = PWDN_GPIO_NUM;
  sensorConfig.pin_reset = RESET_GPIO_NUM;
  sensorConfig.xclk_freq_hz = 20000000;
  sensorConfig.pixel_format = PIXFORMAT_JPEG;
  sensorConfig.frame_size = configuredFrameSize(settings);
  sensorConfig.jpeg_quality = settings.jpegQuality;
  sensorConfig.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  sensorConfig.fb_count = psramFound() ? 2 : 1;
  sensorConfig.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&sensorConfig);
  if (err != ESP_OK) {
    char buf[40];
    snprintf(buf, sizeof(buf), "init failed 0x%X", static_cast<unsigned int>(err));
    cameraError = buf;
    cameraReadyFlag = false;
    return false;
  }

  applyCameraSensorSettings(settings);
  cameraReadyFlag = startCameraStreamServer(&cameraReadyFlag, &cameraError, &streamStats,
                                            authEnabled, sessionToken);
  return cameraReadyFlag;
}
#else
void applyCameraSensorSettings(const CameraConfig&) {}

bool startCameraStreamServer(bool* ready, String* error, CameraStreamStats* stats,
                             const bool*, const char*) {
  if (ready) *ready = false;
  if (error) *error = "Camera disabled on XIAO ESP32-C3";
  if (stats) *stats = CameraStreamStats();
  return false;
}

bool initCameraRuntime(const CameraConfig&, bool& ready, String& error,
                       CameraStreamStats& stats, const bool* auth, const char* token) {
  return startCameraStreamServer(&ready, &error, &stats, auth, token);
}
#endif
