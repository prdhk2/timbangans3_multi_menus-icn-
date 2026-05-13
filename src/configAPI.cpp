#include "config.h"

const char* apiSWD1 = "http://103.82.241.190/api/save-pirolisis"; // API untuk plastik
const char* apiSWD2 = "http://103.82.241.190/api/sampah_hari_ini"; // API untuk botol plastik
const char* apiSWD3 = "http://103.82.241.190/api/incinerator"; // API untuk kertas
// const char* apiSWD4 = "http://103.82.241.190/api/gabruk"; // API untuk gabruk
// const char* apiSWD5 = "http://103.82.241.190/api/kardus"; // API untuk kardus
// const char* apiSWD6 = "http://103.82.241.190/api/botol_kaca"; // API untuk botol kaca
// const char* apiSWD7 = "#"; // API untuk aqua gelas
// const char* apiSWD8 = "#"; // API untuk Berat Incenerator  

int sendData(String menu, float weight) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi tidak terhubung.");
        return -99;
    }

    const char* api = nullptr;
    if (menu == "Pirolisis") api = apiSWD1;
    else if (menu == "Data Sampah Hari ini") api = apiSWD2;
    else if (menu == "Incenerator") api = apiSWD3;
    // else if (menu == "Gabruk") api = apiSWD4;
    // else if (menu == "Kardus") api = apiSWD5;
    // else if (menu == "Botol Kaca") api = apiSWD6;
    // else if (menu == "Aqua Gelas") api = apiSWD7;
    // else if (menu == "Incenerator") api = apiSWD8;

    if (api == nullptr) {
        Serial.println("API Endpoint tidak ditemukan untuk menu: " + menu);
        return -98;
    }

    WiFiClient client;
    HTTPClient http;
    http.begin(client, api);
    http.setTimeout(10000);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{";
    jsonData += "\"user_id\":3,";
    jsonData += "\"weight\":" + String(weight, 2);
    jsonData += "}";

    Serial.println("Payload yang dikirim:");
    Serial.println(jsonData);

    int httpResponseCode = http.POST(jsonData);
    http.end();

    serverResponseDisplay(httpResponseCode);
    return httpResponseCode;
}
