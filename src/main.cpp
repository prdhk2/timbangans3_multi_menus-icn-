#include "config.h"

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 7, 6, 5, U8X8_PIN_NONE);

WiFiState wifiState = WiFiState::DISCONNECTED;
HttpStatus httpState = HTTP_IDLE;
float lastWeight = -1;
float weight = 0.0;

AppState currentState = AppState::MAIN_MENU;

char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

byte rowPins[ROWS] = {8, 9, 10, 11};
byte colPins[COLS] = {12, 13, 14};
Keypad kpd = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

int currentStateEnterButton = HIGH;
bool lastStateEnterButton = HIGH;
int currentStateSendButton = HIGH;
bool lastStateSendButton = HIGH;    
int currentStateBackButton = HIGH;
bool lastStateBackButton = HIGH;

std::vector<MenuData> menuList;
bool isMenuConfirmed = false;
int selectedIndex = 0;
int scrollOffset = 0;
const int maxVisibleItems = 6;
String selectedMenu = "";
String id_menu = "";

unsigned long lastHandleWiFi = 0;
unsigned long lastHandleState = 0;

void setup() {
    disableCore0WDT();
    Serial.begin(115200);

    setupWiFi();

    u8g2.begin();
    u8g2.setBusClock(100000);

    pinMode(SEND_BUTTON, INPUT_PULLUP);
    pinMode(ENTER_BUTTON, INPUT_PULLUP);
    pinMode(BACK_BUTTON, INPUT_PULLUP);

    handleMenuState();

    Serial2.begin(9600, SERIAL_8N1, 18, -1, true);
}

void loop() {
    unsigned long now = millis();

    if (now - lastHandleWiFi >= 200) {
        lastHandleWiFi = now;
        handleWiFi();
    }

    if (now - lastHandleState >= LOOP_INTERVAL_MS) {
        lastHandleState = now;
        handleState();
    }

    yield();
}

void handleState() {
    switch (currentState) {
        case AppState::MAIN_MENU:
            handleMenuState();
            break;
        case AppState::CHOOSE_MENU:
            if (!chooseMenu()) {
            }
            break;
        case AppState::WEIGHT_DISPLAY:
            displayWeight();
            break;
        case AppState::REGISTRATION:
            registerDisplay();
            break;
    }
}