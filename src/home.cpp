#include "config.h"

// Menu state structure
struct MenuState {
    String input;
    unsigned long lastKeypadTime = 0;
    unsigned long lastDebounceTime = 0;
    bool lastButtonState = HIGH;
    bool needsRedraw = true;
};

static MenuState menuState;

void handleMenuState() {
    static bool firstEntry = true;
    
    if (firstEntry) {
        resetMenuState();
        firstEntry = false;
    }

    handleMenuInput();
    
    if (checkForButtonPress()) {
        processMenuSelection();
        firstEntry = true; // Reset untuk next entry
    }

    if (menuState.needsRedraw) {
        updateMenuDisplay();
        menuState.needsRedraw = false;
    }
}

bool checkForButtonPress() {
    static unsigned long lastDebounceTime = 0;
    static bool lastStableState = HIGH;
    static bool lastPhysicalRead = HIGH;

    bool currentRead = digitalRead(ENTER_BUTTON);

    // Deteksi perubahan fisik
    if (currentRead != lastPhysicalRead) {
        lastDebounceTime = millis();
    }

    // Proses setelah debounce period
    if ((millis() - lastDebounceTime) > 50) {
        // Tombol aktif LOW (INPUT_PULLUP)
        if (currentRead == LOW && lastStableState == HIGH) {
            Serial.println("ENTER button pressed");
            lastStableState = currentRead;
            lastPhysicalRead = currentRead;
            return true;
        }
        lastStableState = currentRead;
    }

    lastPhysicalRead = currentRead;
    return false;
}

void processMenuSelection() {
    if (menuState.input == "1") {
        currentState = AppState::REGISTRATION;
        Serial.println("Navigating to registration");
    } 
    else if (menuState.input == "2") {
        currentState = AppState::CHOOSE_MENU;
        selectedIndex = 0;
        scrollOffset = 0; // Reset scroll position
        loadMenuList();
        Serial.println("Navigating to menu selection");
    }
    
    resetMenuState();
}

void handleMenuInput() {
    const unsigned long currentTime = millis();
    
    if (currentTime - menuState.lastKeypadTime >= 50) {
        menuState.lastKeypadTime = currentTime;
        
        char key = kpd.getKey();
        if (key) {
            Serial.printf("Key pressed: %c\n", key);
            
            switch (key) {
                case '*': // Reset input
                    menuState.input = "";
                    menuState.needsRedraw = true;
                    break;
                    
                case '#': // Backspace
                    if (!menuState.input.isEmpty()) {
                        menuState.input.remove(menuState.input.length() - 1);
                        menuState.needsRedraw = true;
                    }
                    break;
                    
                default: // Numeric input
                    if (isdigit(key) && menuState.input.length() < 1) {
                        menuState.input += key;
                        menuState.needsRedraw = true;
                    }
                    break;
            }
        }
    }
}

void updateMenuDisplay() {
    static uint8_t borderOffset = 0;  // Untuk animasi border input
    static unsigned long lastAnimTime = 0;
    
    u8g2.clearBuffer();

    // Status Bar
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x8_tr);

    // WiFi Status Icon (Animasi On/Off)
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
        u8g2.drawStr(105, 8, "[OK]");
    } else {
        u8g2.drawStr(105, 8, "[NC]");
    }

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(5, 25, "1 > Registrasi");
    u8g2.drawStr(5, 40, "2 > Menimbang");

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 52, u8g2.getDisplayWidth(), 12);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setDrawColor(0);
    u8g2.drawStr(5, 61, "Pilih menu:");
    u8g2.drawStr(60, 61, menuState.input.c_str());

    u8g2.sendBuffer();
}

void resetMenuState() {
    menuState.input = "";
    menuState.lastKeypadTime = 0;
    menuState.lastDebounceTime = 0;
    menuState.lastButtonState = HIGH;
    menuState.needsRedraw = true;
}