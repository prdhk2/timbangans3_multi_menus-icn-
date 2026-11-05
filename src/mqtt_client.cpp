#include "mqtt_client.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstdarg>
#include <cstring>
#include <esp_system.h>
#include <time.h>

#include "config.h"
#include "debug_log.h"
#include "json_extract.h"
#include "mqtt_ca_cert.h"
#include "network_service.h"
#include "ota_manager.h"
#include "api_client.h"
#include "app.h"
#include "peripherals.h"
#include "reset_reason.h"

// ResetReason mirrors esp_reset_reason_t by value (reset_reason.h).
static_assert(static_cast<int>(ESP_RST_POWERON) == ResetReason::POWERON, "reset code drift");
static_assert(static_cast<int>(ESP_RST_SW) == ResetReason::SW, "reset code drift");
static_assert(static_cast<int>(ESP_RST_PANIC) == ResetReason::PANIC, "reset code drift");
static_assert(static_cast<int>(ESP_RST_TASK_WDT) == ResetReason::TASK_WDT, "reset code drift");
static_assert(static_cast<int>(ESP_RST_BROWNOUT) == ResetReason::BROWNOUT, "reset code drift");

namespace {

constexpr size_t MAX_LINES_PER_BATCH = 8;  // 8 x 128 chars fits MQTT_PACKET_SIZE
constexpr uint16_t KEEPALIVE_S = 30;       // survives the pauses inside an OTA download
constexpr size_t CMD_BUFFER_SIZE = 512;    // OTA command: ~150-char URL + 64-char hash + keys

char statusTopic[64];
char logTopic[64];
char cmdTopic[64];

volatile bool connected = false;
volatile uint32_t reconnects = 0;
bool confirmedHealthy = false;

// All three are used only from the MQTT task.
char payload[Config::MQTT_PACKET_SIZE];
PubSubClient *activeClient = nullptr;

const char *bootResetName() {
  static const char *name = ResetReason::name(static_cast<int>(esp_reset_reason()));
  return name;
}

unsigned long wallClockOrZero() {
  return wallClockReady() ? static_cast<unsigned long>(time(nullptr)) : 0UL;
}

// Bounded append into `payload`. Returns false once it would overflow.
bool append(size_t &used, const char *format, ...) __attribute__((format(printf, 2, 3)));
bool append(size_t &used, const char *format, ...) {
  if (used >= sizeof(payload)) return false;
  va_list args;
  va_start(args, format);
  int written = vsnprintf(payload + used, sizeof(payload) - used, format, args);
  va_end(args);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(payload) - used) return false;
  used += static_cast<size_t>(written);
  return true;
}

// The retained status: what the fleet dashboard shows as this node's health.
//
//   {"online":true,"uptime_s":..,"fw":"3.0.0","site":"depok_icn_scale","heap":..,
//    "minheap":..,"rssi":..,"ts":..,"reset":"POWERON",
//    "diag":{"kind":"depok_icn_scale","ota_trial":false,
//      "state":"WEIGHING","waste":"Pirolisis","user_id":3,
//      "scale":{"state":"LIVE","age_s":0,"frames":812,"rejected":0,"weight":12.34},
//      "api":{"sends_ok":9,"sends_failed":1,"last_http":200,"last_send_ok_ts":..,"busy":false},
//      "wifi_reconnects":0,"mqtt_reconnects":1,"log_dropped":0}}
//
// scale.state: LIVE (reading within Config::SCALE_STALE_MS), STALE (heard
// before, silent now), OFFLINE (never heard since boot). "weight" is null
// unless LIVE; "waste" is null until a waste type is chosen.
bool buildStatus() {
  const uint32_t now = millis();
  const ScaleSnapshot scale = scaleSnapshot(now);
  const ApiStats api = apiStats();
  const AppStatus app = appStatus();

  size_t used = 0;
  bool ok = append(used,
                   "{\"online\":true,\"uptime_s\":%lu,\"fw\":\"%s\",\"site\":\"%s\","
                   "\"heap\":%u,\"minheap\":%u,\"rssi\":%d,\"ts\":%lu,\"reset\":\"%s\","
                   "\"diag\":{\"kind\":\"%s\",\"ota_trial\":%s,\"state\":\"%s\"",
                   static_cast<unsigned long>(now / 1000UL), Config::FIRMWARE_VERSION, Config::SITE,
                   static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMinFreeHeap()),
                   static_cast<int>(WiFi.RSSI()), wallClockOrZero(), bootResetName(), Config::SITE,
                   OtaManager::onTrial() ? "true" : "false", app.stateName);

  // Waste labels are compile-time ASCII without quotes, so they embed verbatim.
  if (app.wasteIndex >= 0) {
    ok = ok && append(used, ",\"waste\":\"%s\"", Config::WASTE_TYPES[app.wasteIndex].label);
  } else {
    ok = ok && append(used, ",\"waste\":null");
  }
  ok = ok && append(used, ",\"user_id\":%ld", static_cast<long>(Config::UPLOAD_USER_ID));

  const char *scaleState = scale.live ? "LIVE" : (scale.everReceived ? "STALE" : "OFFLINE");
  ok = ok && append(used, ",\"scale\":{\"state\":\"%s\",\"age_s\":%ld,\"frames\":%lu,\"rejected\":%lu",
                    scaleState, scale.everReceived ? static_cast<long>(scale.ageMs / 1000UL) : -1L,
                    static_cast<unsigned long>(scale.frames),
                    static_cast<unsigned long>(scale.rejected));
  if (scale.live) {
    ok = ok && append(used, ",\"weight\":%.2f}", static_cast<double>(scale.weightKg));
  } else {
    ok = ok && append(used, ",\"weight\":null}");
  }

  ok = ok && append(used,
                    ",\"api\":{\"sends_ok\":%lu,\"sends_failed\":%lu,\"last_http\":%d,"
                    "\"last_send_ok_ts\":%lu,\"busy\":%s}",
                    static_cast<unsigned long>(api.sendsOk),
                    static_cast<unsigned long>(api.sendsFailed), api.lastHttpCode,
                    static_cast<unsigned long>(api.lastSendOkEpoch), api.busy ? "true" : "false");

  ok = ok && append(used, ",\"wifi_reconnects\":%lu,\"mqtt_reconnects\":%lu,\"log_dropped\":%lu}}",
                    static_cast<unsigned long>(wifiReconnects()),
                    static_cast<unsigned long>(reconnects),
                    static_cast<unsigned long>(logLinesDropped()));
  return ok;
}

void publishStatus(PubSubClient &client) {
  if (!buildStatus()) {
    // Never publish truncated JSON. Fall back to the minimal shape so the
    // node still shows online and the boot confirmation below still runs.
    logf("[MQTT] status payload overflow, sending minimal status");
    snprintf(payload, sizeof(payload),
             "{\"online\":true,\"uptime_s\":%lu,\"fw\":\"%s\",\"site\":\"%s\",\"heap\":%u,"
             "\"rssi\":%d,\"ts\":%lu,\"reset\":\"%s\"}",
             static_cast<unsigned long>(millis() / 1000UL), Config::FIRMWARE_VERSION, Config::SITE,
             static_cast<unsigned>(ESP.getFreeHeap()), static_cast<int>(WiFi.RSSI()),
             wallClockOrZero(), bootResetName());
  }
  if (!client.publish(statusTopic, payload, true)) return;

  // An image is healthy once it has stayed up a minute AND reached the
  // server; only then does it end an OTA trial (see ota_rollback_logic.h).
  if (!confirmedHealthy && millis() >= Config::BOOT_HEALTHY_AFTER_MS) {
    OtaManager::confirmBootHealthy();
    confirmedHealthy = true;
  }
}

void drainAndPublishLogs(PubSubClient &client) {
  static char lines[MAX_LINES_PER_BATCH][LOG_LINE_CAPACITY + 1];
  size_t count = drainLogLines(lines, MAX_LINES_PER_BATCH);
  if (count == 0) return;

  size_t used = 0;
  bool ok = append(used, "{\"lines\":[");
  for (size_t i = 0; i < count && ok; ++i) {
    // Lines were sanitised by logf(): no quotes, backslashes or control chars.
    ok = append(used, "%s\"%s\"", i ? "," : "", lines[i]);
  }
  ok = ok && append(used, "],\"ts\":%lu}", wallClockOrZero());
  if (ok) client.publish(logTopic, payload);
}

// OTA progress goes straight to the log topic - the log queue cannot be
// drained while this task is busy downloading - as the one fixed-format line
// the server parses (DeviceTelemetryIngestor::OTA_PROGRESS_PATTERN):
//   [OTA] progress stage=downloading pct=42 detail=
void publishOtaProgress(const char *stage, int pct, const char *detail) {
  char line[LOG_LINE_CAPACITY + 1];
  snprintf(line, sizeof(line), "[OTA] progress stage=%s pct=%d detail=%s", stage, pct,
           detail ? detail : "");
  for (char *c = line; *c; ++c) {
    if (*c == '"' || *c == '\\') *c = '\'';
  }
  Serial.println(line);
  if (!activeClient) return;

  char message[LOG_LINE_CAPACITY + 64];
  snprintf(message, sizeof(message), "{\"lines\":[\"%s\"],\"ts\":%lu}", line, wallClockOrZero());
  activeClient->publish(logTopic, message);
}

void handleOta(const char *json) {
  OtaManager::Command command = {};
  char site[24] = {0};
  bool fields = JsonExtract::string(json, "url", command.url, sizeof(command.url)) &&
                JsonExtract::string(json, "sha256", command.sha256, sizeof(command.sha256)) &&
                JsonExtract::string(json, "version", command.version, sizeof(command.version));
  if (!fields) {
    logf("[MQTT] OTA command missing url/sha256/version");
    publishOtaProgress("failed", -1, "OTA command missing url/sha256/version");
    return;
  }
  // The image must be built for this hardware: an OTA meant for another
  // site's gateways would flash firmware that drives these pins wrongly.
  if (!JsonExtract::string(json, "site", site, sizeof(site)) || strcmp(site, Config::SITE) != 0) {
    logf("[OTA] rejected: release is for site '%s', this gateway is '%s'", site, Config::SITE);
    publishOtaProgress("failed", -1, "release is for a different site");
    return;
  }
  // Blocks this task for up to ~90 s by design; the loop task (display,
  // serial links) keeps running. Returns only on failure.
  OtaManager::applyUpdate(command, publishOtaProgress);
}

void handleCommand(const char *json, PubSubClient &client) {
  char cmd[16] = {0};
  if (!JsonExtract::string(json, "cmd", cmd, sizeof(cmd))) {
    logf("[MQTT] cmd payload missing \"cmd\" field");
    return;
  }

  if (strcmp(cmd, "PING") == 0 || strcmp(cmd, "STATUS") == 0) {
    logf("[MQTT] %s received", cmd);
    publishStatus(client);
  } else if (strcmp(cmd, "RESTART") == 0) {
    logf("[MQTT] RESTART requested, rebooting");
    drainAndPublishLogs(client);
    delay(300);
    ESP.restart();
  } else if (strcmp(cmd, "OTA") == 0) {
    handleOta(json);
  } else {
    logf("[MQTT] unrecognised cmd: %s", cmd);
  }
}

void mqttTask(void *) {
  WiFiClientSecure secureClient;
  secureClient.setCACert(ISRG_ROOT_X1_PEM);

  PubSubClient client(secureClient);
  client.setServer(Config::MQTT_BROKER_HOST, Config::MQTT_BROKER_PORT);
  client.setBufferSize(Config::MQTT_PACKET_SIZE);
  client.setKeepAlive(KEEPALIVE_S);
  activeClient = &client;

  client.setCallback([](char *topic, uint8_t *data, unsigned int length) {
    (void)topic;
    static char buffer[CMD_BUFFER_SIZE];
    if (length >= sizeof(buffer)) {
      logf("[MQTT] cmd payload too large (%u bytes), ignored", length);
      return;
    }
    memcpy(buffer, data, length);
    buffer[length] = '\0';
    handleCommand(buffer, *activeClient);
  });

  uint32_t lastConnectAttempt = 0;
  uint32_t lastStatusPublish = 0;
  uint32_t lastLogDrain = 0;
  bool firstAttempt = true;
  bool bootAnnounced = false;

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      connected = false;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (!client.connected()) {
      connected = false;
      uint32_t now = millis();
      if (firstAttempt || now - lastConnectAttempt >= Config::MQTT_RECONNECT_MS) {
        firstAttempt = false;
        lastConnectAttempt = now;
        bool ok = client.connect(Config::DEVICE_ID, Config::MQTT_USERNAME, Config::MQTT_PASSWORD,
                                 statusTopic, 1, true, "{\"online\":false}");
        if (ok) {
          ++reconnects;
          client.subscribe(cmdTopic, 1);
          connected = true;
          logf("[MQTT] connected to %s", Config::MQTT_BROKER_HOST);
          if (!bootAnnounced) {
            bootAnnounced = true;
            logf("[BOOT] fw %s reset=%s", Config::FIRMWARE_VERSION, bootResetName());
          }
          publishStatus(client);
          lastStatusPublish = millis();
        } else {
          logf("[MQTT] connect failed, rc=%d, retry in %lus", client.state(),
               static_cast<unsigned long>(Config::MQTT_RECONNECT_MS / 1000UL));
        }
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    connected = true;
    client.loop();

    uint32_t now = millis();
    if (now - lastStatusPublish >= Config::MQTT_STATUS_INTERVAL_MS) {
      publishStatus(client);
      lastStatusPublish = now;
    }
    if (now - lastLogDrain >= Config::MQTT_LOG_DRAIN_INTERVAL_MS) {
      drainAndPublishLogs(client);
      lastLogDrain = now;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace

void beginMqtt() {
  snprintf(statusTopic, sizeof(statusTopic), "%s/%s/status", Config::SITE, Config::DEVICE_ID);
  snprintf(logTopic, sizeof(logTopic), "%s/%s/log", Config::SITE, Config::DEVICE_ID);
  snprintf(cmdTopic, sizeof(cmdTopic), "%s/%s/cmd", Config::SITE, Config::DEVICE_ID);

  if (!Config::mqttConfigured()) {
    logf("[MQTT] broker credentials not set, remote status/OTA disabled");
    return;
  }
  // TLS + PubSubClient + the OTA download (HTTPClient over TLS) on one task.
  xTaskCreatePinnedToCore(mqttTask, "mqtt", 12288, nullptr, 1, nullptr, 0);
}

bool mqttConnected() { return connected; }
uint32_t mqttReconnects() { return reconnects; }
