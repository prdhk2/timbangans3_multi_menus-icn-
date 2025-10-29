#include "config.h"

// State untuk debounce tombol BACK di Choose Menu
static unsigned long backDebounceTime = 0;
static bool lastBackStableState = HIGH;
static bool lastBackPhysicalRead = HIGH;

// State untuk debounce tombol ENTER di Choose Menu
static unsigned long enterDebounceTime = 0;
static bool lastEnterStableState = HIGH;
static bool lastEnterPhysicalRead = HIGH;

// Timer input
static unsigned long lastInputTime = 0;

// Timeout
const unsigned long chooseMenuTimeout = 30000;

bool chooseMenu() {
    if (lastInputTime == 0) lastInputTime = millis();

    if (checkBackButtonPressedChoose()) {
        Serial.println("BACK pressed - return to MAIN_MENU");
        currentState = AppState::MAIN_MENU;
        resetChooseMenuState();
        return false;
    }

    if (millis() - lastInputTime >= chooseMenuTimeout) {
        Serial.println("Timeout - return to MAIN_MENU");
        currentState = AppState::MAIN_MENU;
        resetChooseMenuState();
        return false;
    }

    char key = kpd.getKey();
    if (key) {
        lastInputTime = millis();

        switch (key) {
            case '8':
                if (selectedIndex < static_cast<int>(menuList.size()) - 1) selectedIndex++;
                if (selectedIndex >= scrollOffset + maxVisibleItems) scrollOffset++;
                Serial.printf("Key 8 pressed - Index %d\n", selectedIndex);
                break;

            case '2':
                if (selectedIndex > 0) selectedIndex--;
                if (selectedIndex < scrollOffset) scrollOffset--;
                Serial.printf("Key 2 pressed - Index %d\n", selectedIndex);
                break;
        }
    }

    if (checkEnterButtonPressedChoose()) {
        selectedMenu = menuList[selectedIndex].name;
        Serial.println("ENTER pressed - Menu selected: " + selectedMenu);
        isMenuConfirmed = true;
        currentState = AppState::WEIGHT_DISPLAY;
        resetChooseMenuState();
        return true;
    }

    updateChooseMenuDisplay();
    return false;
}

bool checkBackButtonPressedChoose() {
    bool currentRead = digitalRead(BACK_BUTTON);

    if (currentRead != lastBackPhysicalRead) {
        backDebounceTime = millis();
    }

    if ((millis() - backDebounceTime) > 50) {
        if (currentRead == LOW && lastBackStableState == HIGH) {
            lastBackStableState = currentRead;
            lastBackPhysicalRead = currentRead;
            return true;
        }
        lastBackStableState = currentRead;
    }

    lastBackPhysicalRead = currentRead;
    return false;
}

bool checkEnterButtonPressedChoose() {
    bool currentRead = digitalRead(ENTER_BUTTON);

    if (currentRead != lastEnterPhysicalRead) {
        enterDebounceTime = millis();
    }

    if ((millis() - enterDebounceTime) > 50) {
        if (currentRead == LOW && lastEnterStableState == HIGH) {
            lastEnterStableState = currentRead;
            lastEnterPhysicalRead = currentRead;
            return true;
        }
        lastEnterStableState = currentRead;
    }

    lastEnterPhysicalRead = currentRead;
    return false;
}

void updateChooseMenuDisplay() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 10);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(25, 8, "Pilih Menu:");
    u8g2.setDrawColor(1);

    int totalItems = menuList.size();
    int endIndex = std::min(scrollOffset + maxVisibleItems, totalItems);

    for (int i = scrollOffset; i < endIndex; i++) {
        String prefix = (i == selectedIndex) ? "> " : "  ";
        String line = prefix + String(menuList[i].id) + ". " + menuList[i].name;
        u8g2.drawStr(2, 20 + ((i - scrollOffset) * 8), line.c_str());
    }

    u8g2.sendBuffer();
}

void resetChooseMenuState() {
    selectedIndex = 0;
    scrollOffset = 0;
    lastInputTime = 0;
    backDebounceTime = 0;
    enterDebounceTime = 0;
    lastBackStableState = HIGH;
    lastBackPhysicalRead = HIGH;
    lastEnterStableState = HIGH;
    lastEnterPhysicalRead = HIGH;
}

void loadMenuList() {
    menuList.clear();
    menuList.push_back({1, "Plastik"});
    menuList.push_back({2, "Botol Plastik"});
    menuList.push_back({3, "Kertas"});
    menuList.push_back({4, "Gabruk"});
    menuList.push_back({5, "Kardus"});
    menuList.push_back({6, "Botol Kaca"});
    menuList.push_back({7, "Aqua Gelas"});
    menuList.push_back({8, "Incenerator"});
}