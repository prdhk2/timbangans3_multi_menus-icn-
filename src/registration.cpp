#include "config.h"

void registerDisplay() {
    static bool firstEnter = true;
    static unsigned long startTime = 0;
    const unsigned long displayDuration = 10000;  // 10 detik

    if (firstEnter) {
        firstEnter = false;
        startTime = millis();
        drawRegisterScreen();  // Gambar pertama kali
    }

    // Jika sudah 10 detik, kembali ke MAIN_MENU
    if (millis() - startTime >= displayDuration) {
        firstEnter = true;  // Reset flag untuk pemanggilan berikutnya
        currentState = AppState::MAIN_MENU;
    }
}

void drawRegisterScreen() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(4, 35, "www.rizqisemesta.com");
    u8g2.sendBuffer();
}
