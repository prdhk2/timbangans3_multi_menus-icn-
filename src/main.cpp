// ESP32-S3 ICN scale ("timbangan ICN, multi menu") - Depok.
//
// Loop (core 1): feed the watchdog, read the scale, housekeep Wi-Fi/NTP, run
// the operator flow (keypad, buttons, LCD), print a periodic summary.
// Nothing in the loop blocks: the upload runs on the API worker task and the
// MQTT/OTA link on its own task, both on core 0.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include "api_client.h"
#include "app.h"
#include "config.h"
#include "debug_log.h"
#include "inputs.h"
#include "mqtt_client.h"
#include "network_service.h"
#include "ota_manager.h"
#include "peripherals.h"
#include "ui.h"

namespace {

uint32_t lastSummaryMs = 0;

void printSummary(uint32_t now) {
  if (now - lastSummaryMs < Config::CONSOLE_SUMMARY_INTERVAL_MS) return;
  lastSummaryMs = now;

  const ScaleSnapshot scale = scaleSnapshot(now);
  const ApiStats api = apiStats();
  const AppStatus app = appStatus();
  logf("[SUM] wifi=%s rssi=%d mqtt=%s state=%s heap=%u", wifiConnected() ? "UP" : "DOWN",
       static_cast<int>(WiFi.RSSI()), mqttConnected() ? "UP" : "DOWN", app.stateName,
       static_cast<unsigned>(ESP.getFreeHeap()));
  logf("[SUM] scale=%s kg=%.2f frames=%lu bad=%lu sends_ok=%lu fail=%lu http=%d",
       scale.live ? "LIVE" : (scale.everReceived ? "STALE" : "OFFLINE"),
       static_cast<double>(scale.live ? scale.weightKg : 0.0f),
       static_cast<unsigned long>(scale.frames), static_cast<unsigned long>(scale.rejected),
       static_cast<unsigned long>(api.sendsOk), static_cast<unsigned long>(api.sendsFailed),
       api.lastHttpCode);
}

}  // namespace

void setup() {
  Serial.begin(Config::CONSOLE_BAUD);
  // Native USB: never wait for a host - the scale must run without a PC.
  Serial.setTxTimeoutMs(0);

  // Before anything else: an OTA image that keeps failing to come up gets
  // replaced by the last confirmed one right here (does not return then).
  OtaManager::checkBootRollback();

  // A hung loop reboots the scale instead of freezing it forever.
  esp_task_wdt_init(Config::WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);

  beginDebugLog();
  beginUi();
  beginInputs();
  beginScale();
  beginNetworkService();
  beginApiClient();
  beginMqtt();
  beginApp();

  logf("[BOOT] %s fw=%s site=%s user_id=%ld", Config::DEVICE_ID, Config::FIRMWARE_VERSION,
       Config::SITE, static_cast<long>(Config::UPLOAD_USER_ID));
  if (!Config::apiKeyConfigured()) {
    logf("[BOOT] WARNING: DP_API_KEY not set, weighing uploads will be rejected (401)");
  }
  if (!Config::mqttConfigured()) {
    logf("[BOOT] WARNING: MQTT credentials not set, remote status and OTA disabled");
  }
}

void loop() {
  esp_task_wdt_reset();
  const uint32_t now = millis();

  updateScale(now);
  updateNetworkService(now);
  updateApp(now);
  printSummary(now);

  delay(1);  // yield; all timing above is millis()-based
}
