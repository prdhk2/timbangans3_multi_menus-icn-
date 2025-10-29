#include "config.h"

const char *ssid = "Biodigester2025";
const char *password = "z3r0w4sTe###";

unsigned long wifiCheckMillis = 0;
bool wifiConnected = false;

#define WIFI_RETRY_INTERVAL 10000

void setupWiFi() {
    Serial.println("Starting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(ssid, password);
    wifiCheckMillis = millis();
}

void handleWiFi() {
    unsigned long now = millis();

    wl_status_t status = WiFi.status();

    // Jika terkoneksi tapi sebelumnya dianggap tidak
    if (status == WL_CONNECTED && !wifiConnected) {
        wifiConnected = true;
        Serial.print("WiFi Connected: ");
        Serial.println(WiFi.localIP());
    }

    // Jika terputus
    if (status != WL_CONNECTED && wifiConnected) {
        wifiConnected = false;
        Serial.println("WiFi lost connection!");
        WiFi.disconnect(true); // Putuskan total
        delay(100);
        WiFi.begin(ssid, password); // Coba sambung lagi
        wifiCheckMillis = now;
    }

    // Jika tidak konek dan cukup waktu berlalu
    if (!wifiConnected && (now - wifiCheckMillis >= WIFI_RETRY_INTERVAL)) {
        Serial.println("Retrying WiFi connection...");
        WiFi.begin(ssid, password);
        wifiCheckMillis = now;
    }
}