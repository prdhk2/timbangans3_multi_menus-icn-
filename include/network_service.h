#pragma once

#include <Arduino.h>

// Wi-Fi (reconnect on a fixed interval, never blocking) and SNTP.
void beginNetworkService();
void updateNetworkService(uint32_t now);

bool wifiConnected();
bool wallClockReady();  // SNTP has produced a real time
uint32_t wifiReconnects();
