#pragma once

#include <Arduino.h>

// ESP32-S3 ICN scale ("timbangan ICN, multi menu") - Depok.
//
//   Scale indicator --UART (RX18, inverted, 9600)--> ESP32-S3
//   4x3 keypad, ENTER / KIRIM / BACK buttons, ST7920 128x64 LCD
//   ESP32-S3 --Wi-Fi--> monitoring.xampah.com
//            HTTPS: weighing upload (POST /api/scavengers/save-*)
//            MQTT:  node status, live debug log, remote commands, OTA
//
// Every pin below is the one the board was built and wired with - unchanged
// from the original firmware.
namespace Config {

// ------------------------------------------------------------------- Pins
namespace Pins {

// ST7920 serial mode via U8g2 software SPI: E (clock), R/W (data), RS (CS).
// The LCD's reset line is not connected.
constexpr uint8_t LCD_CLOCK = 7;
constexpr uint8_t LCD_DATA = 6;
constexpr uint8_t LCD_CS = 5;

constexpr uint8_t KEYPAD_ROW[4] = {8, 9, 10, 11};
constexpr uint8_t KEYPAD_COL[3] = {12, 13, 14};

// Active low, internal pull-up.
constexpr uint8_t ENTER_BUTTON = 4;
constexpr uint8_t SEND_BUTTON = 2;
constexpr uint8_t BACK_BUTTON = 3;

constexpr int8_t SCALE_RX = 18;  // UART2, signal inverted

}  // namespace Pins

constexpr uint32_t CONSOLE_BAUD = 115200;  // native USB CDC
constexpr uint32_t LCD_BUS_CLOCK_HZ = 100000;
constexpr uint32_t SCALE_BAUD = 9600;
constexpr bool SCALE_RX_INVERTED = true;

// ------------------------------------------------------------- Behaviour
// No reading from the scale for this long: the weight shows "N/A" and
// cannot be sent (the last number is no longer what is on the scale).
constexpr uint32_t SCALE_STALE_MS = 3000;
// Smallest weight KIRIM accepts; 0 or a negative (tare) reading is refused.
constexpr float MIN_SEND_WEIGHT_KG = 0.01f;

constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;

constexpr uint32_t REGISTRATION_SCREEN_MS = 10000;
constexpr uint32_t CHOOSE_MENU_TIMEOUT_MS = 30000;  // no key: back to main menu
constexpr uint32_t SEND_RESULT_SCREEN_MS = 3000;

// ------------------------------------------------------------ Waste types
// Menu order, labels and upload endpoints (relative to API_BASE_URL), as
// documented by the backend (app/Config/api/depok.php, config/waste.php):
//   Bahan Incinerator -> save-incenerator  (incenerator, "bahan insinerator 1";
//                        counted as Bahan Insinerator in the MLO formula)
//   Incinerator       -> save-icn-today    (siap_bakar, "siap bakar")
//   Bahan Pirolisis   -> save-bahan-prl    (bahan_pirolisis)
//   Pirolisis         -> save-pirolisis    (pirolisis)
//   Residu            -> save-residu       (residu)
struct WasteType {
  uint8_t id;
  const char *label;
  const char *path;
};

constexpr WasteType WASTE_TYPES[] = {
    {1, "Bahan Incinerator", "/scavengers/save-incenerator"},
    {2, "Incinerator", "/scavengers/save-icn-today"},
    {3, "Bahan Pirolisis", "/scavengers/save-bahan-prl"},
    {4, "Pirolisis", "/scavengers/save-pirolisis"},
    {5, "Residu", "/scavengers/save-residu"},
};
constexpr size_t WASTE_TYPE_COUNT = sizeof(WASTE_TYPES) / sizeof(WASTE_TYPES[0]);

// The account every weighing is booked on (users.id; the server answers 404
// if it does not exist). The original firmware always sent 3 ("admin").
#ifdef DP_USER_ID
constexpr int32_t UPLOAD_USER_ID = DP_USER_ID;
#else
constexpr int32_t UPLOAD_USER_ID = 3;
#endif
constexpr char UPLOAD_USER_LABEL[] = "admin";
static_assert(UPLOAD_USER_ID > 0, "DP_USER_ID must be a positive users.id");

// ----------------------------------------------------------------- Network
// Build secrets come from secrets.ini (see secrets.ini.example and
// scripts/load_secrets.py), never from this file.
#ifdef DP_WIFI_SSID
constexpr char WIFI_SSID[] = DP_WIFI_SSID;
#else
constexpr char WIFI_SSID[] = "Biodigester2025";
#endif
#ifdef DP_WIFI_PASS
constexpr char WIFI_PASSWORD[] = DP_WIFI_PASS;
#else
constexpr char WIFI_PASSWORD[] = "z3r0w4sTe###";
#endif
constexpr uint32_t WIFI_RETRY_MS = 10000;

// Base of every HTTP call; https:// is verified against ISRG Root X1.
#ifdef DP_API_BASE_URL
constexpr char API_BASE_URL[] = DP_API_BASE_URL;
#else
constexpr char API_BASE_URL[] = "https://monitoring.xampah.com/api";
#endif

// X-API-KEY for the weighing upload.
#ifdef DP_API_KEY
constexpr char API_KEY[] = DP_API_KEY;
#else
constexpr char API_KEY[] = "";
#endif

constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t HTTP_TIMEOUT_MS = 10000;

#ifdef DP_DEVICE_ID
constexpr char DEVICE_ID[] = DP_DEVICE_ID;
#else
constexpr char DEVICE_ID[] = "depok-icn-scale-01";
#endif

// Fleet this firmware belongs to: MQTT topic prefix, and an OTA command for
// any other site is refused. Must match config/fleet.php on the server.
constexpr char SITE[] = "depok_icn_scale";

constexpr char FIRMWARE_VERSION[] = "3.1.0";

constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.google.com";
constexpr long GMT_OFFSET_SEC = 7L * 3600L;  // WIB
constexpr int DAYLIGHT_OFFSET_SEC = 0;

// -------------------------------------------------------------------- MQTT
#ifdef DP_MQTT_HOST
constexpr char MQTT_BROKER_HOST[] = DP_MQTT_HOST;
#else
constexpr char MQTT_BROKER_HOST[] = "monitoring.xampah.com";
#endif
#ifdef DP_MQTT_PORT
constexpr uint16_t MQTT_BROKER_PORT = DP_MQTT_PORT;
#else
constexpr uint16_t MQTT_BROKER_PORT = 8883;
#endif
#ifdef DP_MQTT_USER
constexpr char MQTT_USERNAME[] = DP_MQTT_USER;
#else
constexpr char MQTT_USERNAME[] = "";
#endif
#ifdef DP_MQTT_PASS
constexpr char MQTT_PASSWORD[] = DP_MQTT_PASS;
#else
constexpr char MQTT_PASSWORD[] = "";
#endif

#ifdef DP_RELEASE_BUILD
#if !defined(DP_MQTT_USER) || !defined(DP_MQTT_PASS)
#error "Release build needs DP_MQTT_USER and DP_MQTT_PASS (secrets.ini) - without them the device loses MQTT and can no longer be updated over the air."
#endif
#if !defined(DP_API_KEY)
#error "Release build needs DP_API_KEY (secrets.ini) - without it every weighing upload is rejected with HTTP 401."
#endif
#endif

constexpr uint32_t MQTT_RECONNECT_MS = 10000;
constexpr uint32_t MQTT_STATUS_INTERVAL_MS = 10000;
constexpr uint32_t MQTT_LOG_DRAIN_INTERVAL_MS = 1500;

#ifdef MQTT_MAX_PACKET_SIZE
constexpr size_t MQTT_PACKET_SIZE = MQTT_MAX_PACKET_SIZE;
#else
constexpr size_t MQTT_PACKET_SIZE = 2048;
#endif

// An OTA image counts as healthy once it published its MQTT status after
// staying up this long (see OtaManager::confirmBootHealthy()).
constexpr uint32_t BOOT_HEALTHY_AFTER_MS = 60000;

constexpr uint8_t WATCHDOG_TIMEOUT_S = 30;
constexpr uint32_t CONSOLE_SUMMARY_INTERVAL_MS = 60000;

inline bool apiKeyConfigured() { return API_KEY[0] != '\0'; }
inline bool mqttConfigured() {
  return MQTT_BROKER_HOST[0] != '\0' && MQTT_USERNAME[0] != '\0' && MQTT_PASSWORD[0] != '\0';
}

}  // namespace Config
