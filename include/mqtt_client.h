#pragma once

#include <Arduino.h>

// MQTT link to the fleet broker (monitoring.xampah.com:8883, TLS), on its own
// FreeRTOS task pinned to core 0 and deliberately not on the task watchdog:
// an OTA download runs inside this task and may block for up to ~90 s.
//
// Topics (prefix = Config::SITE, id = Config::DEVICE_ID):
//   depok_icn_scale/<id>/status  publish, retained, LWT {"online":false}:
//                                scale, app and upload state (see buildStatus())
//   depok_icn_scale/<id>/log     publish, batched debug_log lines; also OTA progress
//   depok_icn_scale/<id>/cmd     subscribe: {"cmd":"PING"|"STATUS"|"RESTART"|"OTA",...}
//
// Reconnects forever at a fixed interval, like Wi-Fi.
void beginMqtt();

bool mqttConnected();
uint32_t mqttReconnects();
