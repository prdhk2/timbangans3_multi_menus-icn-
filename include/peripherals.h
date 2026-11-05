#pragma once

#include <Arduino.h>

// Scale indicator on UART2, read without blocking from the loop task. The
// MQTT task reads the counters and the latest weight through
// scaleSnapshot(), which copies under a lock.
struct ScaleSnapshot {
  bool everReceived;
  bool live;         // a reading arrived within Config::SCALE_STALE_MS
  float weightKg;    // last reading (meaningful only if everReceived)
  uint32_t ageMs;    // since the last reading; UINT32_MAX if never
  uint32_t frames;   // lines parsed as a weight
  uint32_t rejected; // lines without a usable number
};

void beginScale();
void updateScale(uint32_t now);
ScaleSnapshot scaleSnapshot(uint32_t now);
