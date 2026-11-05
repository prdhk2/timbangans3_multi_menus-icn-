#pragma once

#include <Arduino.h>

// 4x3 keypad and the three buttons (ENTER, KIRIM, BACK), polled once per
// loop pass.
//
// Each button reports one event per press, on the debounced press edge.
// A button that is still held from the previous screen is swallowed with
// ignoreButtonsUntilReleased(), so holding a button through a screen change
// never triggers the next screen's action (the old firmware's "ghost press":
// ENTER on the main menu also picked the first waste type, BACK on the
// weighing screen also left the waste list).
struct InputEvents {
  char key;  // '0'..'9', '*', '#', or 0 for none
  bool enter;
  bool send;
  bool back;
};

void beginInputs();
InputEvents pollInputs(uint32_t now);

void ignoreButtonsUntilReleased();
