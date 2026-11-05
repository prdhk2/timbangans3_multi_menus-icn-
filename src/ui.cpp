#include "ui.h"

#include <U8g2lib.h>
#include <cmath>
#include <cstring>

#include "config.h"

// Geometry notes (128 x 64, text drawn at its baseline; rows used per font):
//   5x8   baseline-6 .. baseline+1      6x10  baseline-7 .. baseline+2
//   6x12  baseline-9 .. baseline+2      7x14B baseline-10 .. baseline+3
// Nothing is drawn on a baseline that would push a glyph past row 63.
namespace {

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, Config::Pins::LCD_CLOCK, Config::Pins::LCD_DATA,
                                 Config::Pins::LCD_CS, U8X8_PIN_NONE);

constexpr uint8_t WIDTH = 128;

void drawCentered(uint8_t y, const char *text) {
  const int w = u8g2.getStrWidth(text);
  u8g2.drawStr(w >= WIDTH ? 0 : (WIDTH - w) / 2, y, text);
}

// Copies text into out, cut to maxChars (with ".." when cut).
void fitText(const char *text, size_t maxChars, char *out, size_t outSize) {
  const size_t len = strlen(text);
  if (len <= maxChars || maxChars < 3) {
    snprintf(out, outSize, "%.*s", static_cast<int>(maxChars), text);
  } else {
    snprintf(out, outSize, "%.*s..", static_cast<int>(maxChars - 2), text);
  }
}

// Inverted 10 px title bar, small font (as on the menu/weight screens).
void drawSmallHeader(const char *title) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, WIDTH, 10);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x8_tr);
  drawCentered(8, title);
  u8g2.setDrawColor(1);
}

}  // namespace

void beginUi() {
  u8g2.begin();
  u8g2.setBusClock(Config::LCD_BUS_CLOCK_HZ);
}

void drawBootScreen(const char *deviceId, const char *version) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, WIDTH, 16);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  drawCentered(13, "TIMBANGAN");
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  drawCentered(34, "Memulai...");
  u8g2.setFont(u8g2_font_5x8_tr);
  char line[26];
  fitText(deviceId, 25, line, sizeof(line));
  drawCentered(50, line);
  snprintf(line, sizeof(line), "FW %s", version);
  drawCentered(60, line);
  u8g2.sendBuffer();
}

void drawMainMenu(const char *input, bool wifiOk) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(105, 8, wifiOk ? "[OK]" : "[NC]");  // x 105..124

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(5, 25, "1 > Registrasi");
  u8g2.drawStr(5, 40, "2 > Menimbang");

  u8g2.drawBox(0, 52, WIDTH, 12);  // y 52..63
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(5, 61, "Pilih menu:");
  u8g2.drawStr(62, 61, input);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

void drawRegistration() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tr);
  drawCentered(35, "www.rizqisemesta.com");  // 120 px
  u8g2.sendBuffer();
}

void drawWasteList(size_t selected, size_t scrollOffset) {
  constexpr size_t VISIBLE = 6;
  u8g2.clearBuffer();
  drawSmallHeader("Pilih Menu:");

  // Rows at baselines 17..62, highlight y-7..y+1 (10..63).
  const size_t end = scrollOffset + VISIBLE < Config::WASTE_TYPE_COUNT ? scrollOffset + VISIBLE
                                                                       : Config::WASTE_TYPE_COUNT;
  for (size_t i = scrollOffset; i < end; ++i) {
    const uint8_t y = 17 + static_cast<uint8_t>(i - scrollOffset) * 9;
    if (i == selected) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 7, WIDTH, 9);
      u8g2.setDrawColor(0);
    }
    char line[40];
    char label[26];
    fitText(Config::WASTE_TYPES[i].label, 21, label, sizeof(label));
    snprintf(line, sizeof(line), "%u. %s", Config::WASTE_TYPES[i].id, label);  // <= 25 chars
    u8g2.drawStr(2, y, line);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

void drawWeighing(const char *wasteLabel, bool live, float weightKg, bool wifiOk) {
  u8g2.clearBuffer();
  drawSmallHeader("DISPLAY TIMBANGAN");

  // Labels x 2..31, colon x 35, value from x 40: 17 chars of 5x8 (x 40..124).
  char value[20];
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(2, 20, "USER");
  u8g2.drawStr(35, 20, ":");
  snprintf(value, sizeof(value), "%ld (%s)", static_cast<long>(Config::UPLOAD_USER_ID),
           Config::UPLOAD_USER_LABEL);
  u8g2.drawStr(40, 20, value);

  u8g2.drawStr(2, 30, "MENU");
  u8g2.drawStr(35, 30, ":");
  fitText(wasteLabel, 17, value, sizeof(value));
  u8g2.drawStr(40, 30, value);

  u8g2.drawStr(2, 48, "WEIGHT");
  u8g2.drawStr(35, 48, ":");

  // Weight right-aligned to x 106, never wider than 64 px (x 42..106), so
  // it can reach neither the ':' nor the "Kg".
  constexpr int VALUE_RIGHT = 106;
  constexpr int VALUE_MAX_WIDTH = 64;
  u8g2.setFont(u8g2_font_ncenB14_tr);
  char number[16] = "N/A";
  if (live && std::isfinite(weightKg)) {
    bool fits = false;
    for (int decimals = 2; decimals >= 0 && !fits; --decimals) {
      snprintf(number, sizeof(number), "%.*f", decimals, static_cast<double>(weightKg));
      fits = u8g2.getStrWidth(number) <= VALUE_MAX_WIDTH;
    }
    if (!fits) snprintf(number, sizeof(number), "OVR");
  }
  u8g2.drawStr(VALUE_RIGHT - u8g2.getStrWidth(number), 50, number);
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(110, 50, "Kg");

  // Footer (y 56..63): KIRIM button on the left as before, status right.
  constexpr uint8_t BUTTON_W = 40;
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawBox(0, 56, BUTTON_W, 8);
  u8g2.setDrawColor(0);
  u8g2.drawStr(7, 63, "KIRIM");
  u8g2.setDrawColor(1);
  const char *status = !live ? "Timbangan N/A" : (!wifiOk ? "WiFi putus" : "");
  if (status[0]) u8g2.drawStr(WIDTH - 2 - u8g2.getStrWidth(status), 63, status);
  u8g2.sendBuffer();
}

void drawSending(uint8_t dots) {
  u8g2.clearBuffer();
  u8g2.drawFrame(2, 18, 124, 34);
  u8g2.setFont(u8g2_font_helvB12_tr);
  drawCentered(38, "Mengirim");
  u8g2.setFont(u8g2_font_6x10_tr);
  static const char *const DOTS[] = {"", ".", "..", "..."};
  drawCentered(48, DOTS[dots % 4]);
  u8g2.sendBuffer();
}

void drawSendResult(bool ok, const char *detail) {
  u8g2.clearBuffer();
  u8g2.drawFrame(2, 18, 124, 34);  // y 18..51
  u8g2.setFont(u8g2_font_helvB12_tr);
  if (ok) {
    drawCentered(40, "Data Terkirim!");
  } else {
    drawCentered(33, "Gagal!");
    u8g2.setFont(u8g2_font_6x10_tr);
    char line[20];
    fitText(detail ? detail : "", 19, line, sizeof(line));  // inside the frame
    drawCentered(46, line);
  }
  u8g2.sendBuffer();
}
