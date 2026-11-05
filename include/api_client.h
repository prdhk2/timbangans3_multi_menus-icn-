#pragma once

#include <Arduino.h>

// Weighing uploads to the monitoring server, run one at a time on a worker
// task (core 0) so a slow TLS handshake never freezes the LCD or the buttons.
//
// The loop submits a request and polls for its result; every result carries
// the sequence number of its request, so the app can ignore an answer that
// arrives after it has moved on.
//
//   POST {API_BASE_URL}/scavengers/save-*   X-API-KEY
//        {"user_id":N,"weight":12.34}

enum class SendVerdict : uint8_t {
  OK,        // 2xx
  REJECTED,  // 4xx: bad key, unknown user, validation - retrying won't help
  ERROR,     // no answer / 5xx
};

struct ApiResult {
  uint32_t seq;
  int httpCode;  // < 0: transport error (HTTPClient code)
  SendVerdict send;
};

struct ApiStats {
  uint32_t sendsOk;
  uint32_t sendsFailed;
  int lastHttpCode;         // 0 before the first request
  uint32_t lastSendOkEpoch; // 0 if none (or before NTP)
  bool busy;
};

void beginApiClient();

// Returns the request's sequence number, or 0 if one is already in flight
// (or the arguments are invalid).
uint32_t apiRequestSend(size_t wasteIndex, float weightKg);

// True (and fills out) when a result is ready.
bool apiTakeResult(ApiResult &out);

ApiStats apiStats();
