#include "config.h"

// Constants for display configuration
constexpr uint8_t HEADER_HEIGHT = 10;
constexpr uint8_t FOOTER_HEIGHT = 8;
constexpr uint8_t DEBOUNCE_DELAY_MS = 50;
constexpr uint8_t WEIGHT_DISPLAY_X = 40;
constexpr uint8_t WEIGHT_DISPLAY_Y = 50;
constexpr uint8_t MAX_MENU_DISPLAY_CHARS = 15;

namespace {
    // Button state structure
    struct ButtonState {
        unsigned long lastDebounceTime = 0;
        bool lastStableState = HIGH;
        bool lastPhysicalRead = HIGH;
    };
    
    // Button instances
    ButtonState backButtonState;
    ButtonState sendButtonState;

    // Generic button check template
    template<typename Action>
    bool checkButton(uint8_t pin, ButtonState& state, Action action) {
        const bool currentRead = digitalRead(pin);

        if (currentRead != state.lastPhysicalRead) {
            state.lastDebounceTime = millis();
        }

        if ((millis() - state.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
            if (currentRead == LOW && state.lastStableState == HIGH) {
                state.lastStableState = currentRead;
                state.lastPhysicalRead = currentRead;
                return action();
            }
            state.lastStableState = currentRead;
        }

        state.lastPhysicalRead = currentRead;
        return false;
    }
}

void displayWeight() {
    static bool firstEnter = true;
    static unsigned long lastUpdate = 0;
    constexpr unsigned long UPDATE_INTERVAL_MS = 500;

    if (firstEnter) {
        firstEnter = false;
        lastWeight = -1;
        updateDisplay(readWeight());
    }

    const float currentWeight = readWeight();
    const unsigned long currentTime = millis();

    if (currentWeight != lastWeight || currentTime - lastUpdate >= UPDATE_INTERVAL_MS) {
        lastUpdate = currentTime;
        lastWeight = currentWeight;
        weight = currentWeight;
        updateDisplay(currentWeight);
    }

    if (checkSendButton()) {
        firstEnter = true;
    } 
    
    if (checkBackButtonPressedWeight()) {
        firstEnter = true;
        currentState = AppState::CHOOSE_MENU;
    }
}

void updateDisplay(float weightValue) {
    u8g2.clearBuffer();
    drawHeader();
    drawUserInfo();
    drawMenuInfo();
    drawWeightValue(weightValue);
    drawFooterButtons();
    u8g2.sendBuffer();
    yield();
}

void drawHeader() {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, u8g2.getDisplayWidth(), HEADER_HEIGHT);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(23, HEADER_HEIGHT - 2, "DISPLAY TIMBANGAN");
    u8g2.setDrawColor(1);
}

void drawUserInfo() {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(2, 20, "USER");
    u8g2.drawStr(35, 20, ":");
    u8g2.drawStr(40, 20, "3 (admin)");
}

void drawMenuInfo() {
    u8g2.drawStr(2, 30, "MENU");
    u8g2.drawStr(35, 30, ":");
    const String displayedMenu = selectedMenu.length() <= MAX_MENU_DISPLAY_CHARS 
        ? selectedMenu 
        : selectedMenu.substring(0, MAX_MENU_DISPLAY_CHARS);
    u8g2.drawStr(40, 30, displayedMenu.c_str());
}

void drawWeightValue(float weightValue) {
    u8g2.drawStr(2, 48, "WEIGHT");
    u8g2.drawStr(35, 48, ":");
    char weightStr[10];
    if (isnan(weightValue)) {
        strncpy(weightStr, "N/A", sizeof(weightStr));
    } else {
        dtostrf(weightValue, 4, 2, weightStr);
    }

    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.setCursor(WEIGHT_DISPLAY_X, WEIGHT_DISPLAY_Y);
    u8g2.print(weightStr);
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(110, WEIGHT_DISPLAY_Y, "Kg");
}

void drawFooterButtons() {
    const uint8_t buttonWidth = 40;
    const uint8_t rightButtonX = u8g2.getDisplayWidth() - buttonWidth - 1;
    
    u8g2.drawBox(0, u8g2.getDisplayHeight() - FOOTER_HEIGHT, buttonWidth, FOOTER_HEIGHT);
    
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(7, u8g2.getDisplayHeight() - 1, "KIRIM");
    u8g2.setDrawColor(1);
}

bool checkSendButton() {
    return checkButton(SEND_BUTTON, sendButtonState, []() {
        if (httpState == HTTP_IDLE) {
            Serial.println("Tombol 'Kirim' ditekan, mengirim data...");
            const int response = sendData(selectedMenu, weight);
            Serial.printf("Response Code: %d\n", response);
            serverResponseDisplay(response);
            return true;
        }
        return false;
    });
}

bool checkBackButtonPressedWeight() {
    return checkButton(BACK_BUTTON, backButtonState, []() {
        return true; // Just return true for back button
    });
}

void resetButtons() {
    static bool isWaitingRelease = true;

    if (isWaitingRelease && 
        digitalRead(SEND_BUTTON) == HIGH && 
        digitalRead(BACK_BUTTON) == HIGH) {
        isWaitingRelease = false;
    }
}