#pragma once

#include <Arduino.h>

// Bounded, non-blocking mirror of the serial console onto the MQTT debug
// stream (see mqtt_client.h). logf() prints the line to Serial exactly like
// Serial.printf() would, AND queues a copy for mqtt_client.cpp to batch-
// publish to <site>/<id>/log - so the dashboard's live log tail shows what a
// technician with a USB cable would see. The console is the ESP32-S3's
// native USB (CDC); lines are dropped, never blocked on, when no host is
// attached.
void beginDebugLog();

// Safe before beginDebugLog() and while MQTT is down: the line is printed and,
// once the queue exists, queued (oldest dropped when full).
void logf(const char *format, ...) __attribute__((format(printf, 1, 2)));

// One formatted line, capped so a single line can never eat the whole
// MQTT_PACKET_SIZE budget once batched with others.
constexpr size_t LOG_LINE_CAPACITY = 127;
constexpr size_t LOG_QUEUE_DEPTH = 32;

// Drains up to maxLines queued lines into out[], returning how many were
// copied. Never blocks.
size_t drainLogLines(char out[][LOG_LINE_CAPACITY + 1], size_t maxLines);

uint32_t logLinesQueued();
uint32_t logLinesDropped();
