#pragma once

#include <Arduino.h>

// The operator-facing flow, as in the original firmware:
//
//   MAIN_MENU --"1"+ENTER--> REGISTRATION --10 s / BACK--> MAIN_MENU
//   MAIN_MENU --"2"+ENTER--> CHOOSE_MENU --ENTER--> WEIGHING
//   WEIGHING --KIRIM--> SENDING --> SEND_RESULT --3 s--> WEIGHING
//
// Ways back: BACK on the waste list returns to the main menu (also after
// 30 s without a key); BACK on the weighing screen returns to the waste list.
enum class AppState : uint8_t {
  MAIN_MENU,
  REGISTRATION,
  CHOOSE_MENU,
  WEIGHING,
  SENDING,
  SEND_RESULT,
};

void beginApp();
void updateApp(uint32_t now);

// For the MQTT status (read from another task; copied under a lock).
struct AppStatus {
  AppState state;
  const char *stateName;
  int wasteIndex;  // -1 when none chosen
};
AppStatus appStatus();
