#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Keypad.h>
#include "esp_task_wdt.h"
#include <algorithm>
#include <vector>

// WiFi
extern const char* ssid;
extern const char* password;

enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

extern WiFiState wifiState;

// HTTP
enum HttpStatus { 
    HTTP_IDLE, 
    HTTP_SENDING, 
    HTTP_RECEIVING, 
    HTTP_ERROR 
};

extern HttpStatus httpState;

enum class AppState {
    MAIN_MENU,
    CHOOSE_MENU,
    WEIGHT_DISPLAY,
    REGISTRATION
};

extern AppState currentState;

// API Endpoints
extern const char* apiSWD1;
extern const char* apiSWD2;
extern const char* apiSWD3;
extern const char* apiSWD4;
extern const char* apiSWD5;
extern const char* apiSWD6;

// Menu Struct
struct MenuData {
    int id;
    String name;
};
extern std::vector<MenuData> menuList;
extern bool isMenuConfirmed;
extern String menuname;
extern int selectedIndex;
extern String id_menu;
extern int scrollOffset;
extern const int maxVisibleItems;
extern String selectedMenu;

// Display
extern U8G2_ST7920_128X64_F_SW_SPI u8g2;

// Keypad
const byte ROWS = 4;
const byte COLS = 3;
extern char hexaKeys[ROWS][COLS];
extern byte rowPins[ROWS];
extern byte colPins[COLS];
extern Keypad kpd;

// Buttons
#define ENTER_BUTTON 4
#define SEND_BUTTON 2
#define BACK_BUTTON 3

extern int currentStateEnterButton;
extern bool lastStateEnterButton;
extern int currentStateSendButton;
extern bool lastStateSendButton;
extern int currentStateBackButton;
extern bool lastStateBackButton;

// Weight
extern float lastWeight;
extern float weight;

constexpr unsigned long LOOP_INTERVAL_MS = 10;

// Function Declaration
void setupWiFi();
void handleWiFi();
void handleState();
void registerDisplay();
void drawRegisterScreen();
void handleMenuState();
void resetMenuState();
void handleMenuInput();
bool checkForButtonPress();
void processMenuSelection();
bool checkBackButtonPressedChoose();
void resetChooseMenuState();
void updateMenuDisplay();
bool chooseMenu();
void updateChooseMenuDisplay();
bool checkEnterButtonPressedChoose();
void loadMenuList();
float readWeight();
void displayWeight();
void drawHeader();
void drawUserInfo();
void drawMenuInfo();
void drawWeightValue(float weightValue);
void drawFooterButtons();
bool checkBackButtonPressedWeight();
void updateDisplay(float weight);
int sendData(String menu, float weight);
void serverResponseDisplay(int responseCode);
String extractNumericPart(String inputString);
bool checkSendButton();
void resetButtons();
bool checkKeyPress();

#endif