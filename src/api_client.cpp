#include "api_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <time.h>

#include "config.h"
#include "debug_log.h"
#include "mqtt_ca_cert.h"
#include "network_service.h"

namespace {

struct Job {
  uint32_t seq;
  size_t wasteIndex;
  float weightKg;
};

QueueHandle_t jobQueue = nullptr;
QueueHandle_t resultQueue = nullptr;
volatile bool busy = false;
uint32_t nextSeq = 1;

portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;
ApiStats stats = {};

// Printable ASCII only, without quotes or backslashes: the text ends up in a
// log line that is embedded in MQTT JSON.
void copySafe(const char *in, char *out, size_t outSize) {
  size_t n = 0;
  for (; in && *in && n + 1 < outSize; ++in) {
    const unsigned char c = static_cast<unsigned char>(*in);
    out[n++] = (c < 0x20 || c >= 0x7F || c == '"' || c == '\\') ? '?' : static_cast<char>(c);
  }
  out[n] = '\0';
}

bool isHttps() { return strncmp(Config::API_BASE_URL, "https://", 8) == 0; }

// One POST. Fills code and (for a response with a body) body, up to
// bodySize - 1 bytes.
void post(const char *url, const char *payload, int &code, char *body, size_t bodySize) {
  body[0] = '\0';
  if (!wifiConnected()) {
    code = HTTPC_ERROR_CONNECTION_REFUSED;
    return;
  }

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  http.setConnectTimeout(Config::HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(Config::HTTP_TIMEOUT_MS);
  http.setReuse(false);

  bool begun;
  if (isHttps()) {
    secureClient.setCACert(ISRG_ROOT_X1_PEM);
    begun = http.begin(secureClient, url);
  } else {
    begun = http.begin(plainClient, url);
  }
  if (!begun) {
    code = HTTPC_ERROR_CONNECTION_REFUSED;
    logf("[API] bad url: %.80s", url);
    return;
  }

  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  if (Config::apiKeyConfigured()) http.addHeader("X-API-KEY", Config::API_KEY);

  // POST() takes a non-const pointer but only reads from it.
  code = http.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(payload)), strlen(payload));

  // Small JSON answers from our own server; anything larger is not read.
  constexpr int MAX_BODY = 4096;
  if (code > 0 && http.getSize() <= MAX_BODY) {
    const String response = http.getString();
    strncpy(body, response.c_str(), bodySize - 1);
    body[bodySize - 1] = '\0';
  }
  http.end();
}

void doSend(const Job &job, ApiResult &result) {
  const Config::WasteType &type = Config::WASTE_TYPES[job.wasteIndex];
  char url[160];
  char payload[64];
  snprintf(url, sizeof(url), "%s%s", Config::API_BASE_URL, type.path);
  snprintf(payload, sizeof(payload), "{\"user_id\":%ld,\"weight\":%.2f}",
           static_cast<long>(Config::UPLOAD_USER_ID), static_cast<double>(job.weightKg));

  static char body[512];  // worker task only
  int code = 0;
  post(url, payload, code, body, sizeof(body));
  result.httpCode = code;

  if (code >= 200 && code < 300) result.send = SendVerdict::OK;
  else if (code >= 400 && code < 500) result.send = SendVerdict::REJECTED;
  else result.send = SendVerdict::ERROR;

  portENTER_CRITICAL(&statsMux);
  stats.lastHttpCode = code;
  if (result.send == SendVerdict::OK) {
    ++stats.sendsOk;
    const time_t t = time(nullptr);
    stats.lastSendOkEpoch = wallClockReady() ? static_cast<uint32_t>(t) : 0;
  } else {
    ++stats.sendsFailed;
  }
  portEXIT_CRITICAL(&statsMux);

  if (result.send == SendVerdict::OK) {
    logf("[SEND] %s %.2fkg user_id=%ld http=%d", type.label, static_cast<double>(job.weightKg),
         static_cast<long>(Config::UPLOAD_USER_ID), code);
  } else {
    char reason[72];
    copySafe(body, reason, sizeof(reason));
    logf("[SEND] FAILED %s %.2fkg user_id=%ld http=%d %s", type.label,
         static_cast<double>(job.weightKg), static_cast<long>(Config::UPLOAD_USER_ID), code,
         reason);
    if (code == 401 || code == 403) logf("[SEND] upload refused - check DP_API_KEY");
    if (code == 404) logf("[SEND] server says not found - check DP_USER_ID exists in users");
  }
}

void workerTask(void *) {
  for (;;) {
    Job job;
    if (xQueueReceive(jobQueue, &job, portMAX_DELAY) != pdTRUE) continue;

    ApiResult result = {};
    result.seq = job.seq;
    doSend(job, result);

    xQueueOverwrite(resultQueue, &result);
  }
}

}  // namespace

void beginApiClient() {
  jobQueue = xQueueCreate(1, sizeof(Job));
  resultQueue = xQueueCreate(1, sizeof(ApiResult));
  if (!jobQueue || !resultQueue) {
    logf("[API] queue allocation failed");
    return;
  }
  // HTTPClient over TLS: 12 KB leaves ample headroom.
  xTaskCreatePinnedToCore(workerTask, "api", 12288, nullptr, 1, nullptr, 0);
}

uint32_t apiRequestSend(size_t wasteIndex, float weightKg) {
  if (!jobQueue || busy || wasteIndex >= Config::WASTE_TYPE_COUNT) return 0;
  Job job = {};
  job.seq = nextSeq++;
  if (nextSeq == 0) nextSeq = 1;  // 0 means "rejected"
  job.wasteIndex = wasteIndex;
  job.weightKg = weightKg;
  busy = true;
  if (xQueueSend(jobQueue, &job, 0) != pdTRUE) {
    busy = false;
    return 0;
  }
  return job.seq;
}

bool apiTakeResult(ApiResult &out) {
  if (!resultQueue || xQueueReceive(resultQueue, &out, 0) != pdTRUE) return false;
  busy = false;
  return true;
}

ApiStats apiStats() {
  portENTER_CRITICAL(&statsMux);
  ApiStats copy = stats;
  portEXIT_CRITICAL(&statsMux);
  copy.busy = busy;
  return copy;
}
