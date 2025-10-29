#include "config.h"

// Fungsi untuk mengambil teks dari PROGMEM dan menggambarnya di tengah layar
void drawCenteredPStr(u8g2_uint_t y, const char* pstr) {
    char buffer[32]; // Ukuran buffer yang cukup
    strncpy_P(buffer, pstr, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

    u8g2_uint_t x = (128 - u8g2.getStrWidth(buffer)) / 2;
    u8g2.setCursor(x, y);
    u8g2.print(buffer);
}

void serverResponseDisplay(int responseCode) {
    unsigned long startTime = millis();

    do {
        // Teks dari PROGMEM
        static const char successMsg[] PROGMEM = "Data Terkirim!";
        static const char failMsg[] PROGMEM = "Gagal!";
        static const char codePrefix[] PROGMEM = "Kode: ";
    
        u8g2.clearBuffer();
        u8g2.setDrawColor(0);
        u8g2.drawBox(0, 20, 128, 30);
        u8g2.setDrawColor(1);
        u8g2.drawFrame(2, 18, 124, 34);
    
        u8g2.setFont(u8g2_font_helvB12_tr);

        if (responseCode == HTTP_CODE_OK) {
            drawCenteredPStr(40, successMsg);
        } else {
            drawCenteredPStr(30, failMsg);

            char codeBuffer[32];
            char prefixBuffer[16];
            strncpy_P(prefixBuffer, codePrefix, sizeof(prefixBuffer));
            prefixBuffer[sizeof(prefixBuffer) - 1] = '\0';

            snprintf(codeBuffer, sizeof(codeBuffer), "%s%d", prefixBuffer, responseCode);

            u8g2.setCursor((128 - u8g2.getStrWidth(codeBuffer)) / 2, 45);
            u8g2.print(codeBuffer);
        }

        u8g2.sendBuffer();
        yield();

    } while (millis() - startTime < 3000);

    currentState = AppState::WEIGHT_DISPLAY;
}