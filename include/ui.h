#pragma once

#include <Arduino.h>

// ST7920 128x64 screens. Each draw*() call renders one full frame; the app
// decides when to call them. Layouts are the original firmware's, with every
// string bounded so nothing overlaps or runs off the panel.
void beginUi();

void drawBootScreen(const char *deviceId, const char *version);
void drawMainMenu(const char *input, bool wifiOk);
void drawRegistration();
void drawWasteList(size_t selected, size_t scrollOffset);

// weightKg is ignored when !live (shows "N/A").
void drawWeighing(const char *wasteLabel, bool live, float weightKg, bool wifiOk);
void drawSending(uint8_t dots);
// ok: "Data Terkirim!". Otherwise "Gagal!" plus detail (e.g. "Kode: 422").
void drawSendResult(bool ok, const char *detail);
