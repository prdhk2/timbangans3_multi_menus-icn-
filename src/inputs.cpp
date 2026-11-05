#include "inputs.h"

#include <Keypad.h>

#include "config.h"

namespace {

char keymap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'},
};

// The Keypad library takes non-const pin arrays.
byte rowPins[4] = {Config::Pins::KEYPAD_ROW[0], Config::Pins::KEYPAD_ROW[1],
                   Config::Pins::KEYPAD_ROW[2], Config::Pins::KEYPAD_ROW[3]};
byte colPins[3] = {Config::Pins::KEYPAD_COL[0], Config::Pins::KEYPAD_COL[1],
                   Config::Pins::KEYPAD_COL[2]};

Keypad keypad(makeKeymap(keymap), rowPins, colPins, 4, 3);

// One debounced, active-low button. Its state always follows the pin -
// never reset to "released" while the button is physically held.
struct Button {
  uint8_t pin;
  bool stablePressed;
  bool lastRaw;
  uint32_t lastRawChangeMs;
  bool ignoreUntilRelease;

  void begin() {
    pinMode(pin, INPUT_PULLUP);
    stablePressed = lastRaw = digitalRead(pin) == LOW;
    lastRawChangeMs = millis();
    // Powered up with the button held: do not count it as a press.
    ignoreUntilRelease = stablePressed;
  }

  // True once per press, on the debounced press edge.
  bool update(uint32_t now) {
    const bool raw = digitalRead(pin) == LOW;
    if (raw != lastRaw) {
      lastRaw = raw;
      lastRawChangeMs = now;
    }
    if (now - lastRawChangeMs < Config::BUTTON_DEBOUNCE_MS || raw == stablePressed) return false;

    stablePressed = raw;
    if (!stablePressed) {
      ignoreUntilRelease = false;
      return false;
    }
    return !ignoreUntilRelease;
  }

  void ignoreWhileHeld() {
    if (stablePressed) ignoreUntilRelease = true;
  }
};

Button enterButton = {Config::Pins::ENTER_BUTTON, false, false, 0, false};
Button sendButton = {Config::Pins::SEND_BUTTON, false, false, 0, false};
Button backButton = {Config::Pins::BACK_BUTTON, false, false, 0, false};

}  // namespace

void beginInputs() {
  enterButton.begin();
  sendButton.begin();
  backButton.begin();
  keypad.setDebounceTime(Config::BUTTON_DEBOUNCE_MS);
}

InputEvents pollInputs(uint32_t now) {
  InputEvents events;
  events.key = keypad.getKey();
  events.enter = enterButton.update(now);
  events.send = sendButton.update(now);
  events.back = backButton.update(now);
  return events;
}

void ignoreButtonsUntilReleased() {
  enterButton.ignoreWhileHeld();
  sendButton.ignoreWhileHeld();
  backButton.ignoreWhileHeld();
}
