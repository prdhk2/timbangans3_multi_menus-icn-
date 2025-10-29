#include "config.h"

void showErrorMessage(const String& msg) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(10, 30, msg.c_str());
    u8g2.sendBuffer();
}