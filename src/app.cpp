#include "app.h"

#include <cmath>
#include <cstring>

#include "api_client.h"
#include "config.h"
#include "debug_log.h"
#include "inputs.h"
#include "network_service.h"
#include "peripherals.h"
#include "ui.h"

namespace {

constexpr size_t VISIBLE_MENU_ROWS = 6;
constexpr uint32_t ANIMATION_MS = 300;
constexpr uint32_t WEIGHING_REFRESH_MS = 250;
constexpr uint32_t STATIC_REFRESH_MS = 1000;  // screens without live data
// Hard ceiling on waiting for the server, above the HTTP connect + read
// timeouts. A late answer after this is dropped (sequence mismatch).
constexpr uint32_t API_WAIT_LIMIT_MS = 30000;

AppState state = AppState::MAIN_MENU;
uint32_t stateEnteredMs = 0;
uint32_t lastDrawMs = 0;
bool redraw = true;

// Main menu
char menuInput[2] = "";

// Waste list
size_t selectedIndex = 0;
size_t scrollOffset = 0;
uint32_t lastListKeyMs = 0;

// Chosen waste type
int wasteIndex = -1;

// In-flight upload and its outcome
uint32_t pendingSeq = 0;
bool sendOk = false;
char sendDetail[24] = "";

uint8_t animationFrame = 0;

portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;

const char *stateName(AppState s) {
  switch (s) {
    case AppState::MAIN_MENU: return "MAIN_MENU";
    case AppState::REGISTRATION: return "REGISTRATION";
    case AppState::CHOOSE_MENU: return "CHOOSE_MENU";
    case AppState::WEIGHING: return "WEIGHING";
    case AppState::SENDING: return "SENDING";
    case AppState::SEND_RESULT: return "SEND_RESULT";
  }
  return "UNKNOWN";
}

void setWaste(int index) {
  portENTER_CRITICAL(&statusMux);
  wasteIndex = index;
  portEXIT_CRITICAL(&statusMux);
}

const char *wasteLabel() {
  return wasteIndex >= 0 ? Config::WASTE_TYPES[wasteIndex].label : "";
}

void enter(AppState next, uint32_t now) {
  if (next != state) {
    logf("[APP] %s -> %s", stateName(state), stateName(next));
  }
  portENTER_CRITICAL(&statusMux);
  state = next;
  portEXIT_CRITICAL(&statusMux);
  stateEnteredMs = now;
  redraw = true;
  animationFrame = 0;
  // A press that caused this change must not also act on the new screen.
  ignoreButtonsUntilReleased();

  switch (next) {
    case AppState::MAIN_MENU:
      menuInput[0] = '\0';
      setWaste(-1);
      break;
    case AppState::CHOOSE_MENU:
      lastListKeyMs = now;
      break;
    default:
      break;
  }
}

bool due(uint32_t now, uint32_t intervalMs) { return redraw || now - lastDrawMs >= intervalMs; }

void markDrawn(uint32_t now) {
  redraw = false;
  lastDrawMs = now;
}

// ------------------------------------------------------------------ states

void runMainMenu(const InputEvents &in, uint32_t now) {
  if (in.key == '*' || in.key == '#') {
    menuInput[0] = '\0';  // one-character field: reset and backspace both clear it
    redraw = true;
  } else if (in.key >= '0' && in.key <= '9') {
    menuInput[0] = in.key;
    menuInput[1] = '\0';
    redraw = true;
  }

  if (in.enter) {
    if (menuInput[0] == '1') {
      enter(AppState::REGISTRATION, now);
      return;
    }
    if (menuInput[0] == '2') {
      selectedIndex = 0;
      scrollOffset = 0;
      enter(AppState::CHOOSE_MENU, now);
      return;
    }
  }

  // Redrawn every second too, so the Wi-Fi marker follows the link.
  if (due(now, STATIC_REFRESH_MS)) {
    drawMainMenu(menuInput, wifiConnected());
    markDrawn(now);
  }
}

void runRegistration(const InputEvents &in, uint32_t now) {
  if (now - stateEnteredMs >= Config::REGISTRATION_SCREEN_MS || in.back) {
    enter(AppState::MAIN_MENU, now);
    return;
  }
  if (redraw) {
    drawRegistration();
    markDrawn(now);
  }
}

void runChooseMenu(const InputEvents &in, uint32_t now) {
  const size_t count = Config::WASTE_TYPE_COUNT;

  if (in.back) {
    enter(AppState::MAIN_MENU, now);
    return;
  }
  if (in.key) lastListKeyMs = now;
  if (now - lastListKeyMs >= Config::CHOOSE_MENU_TIMEOUT_MS) {
    logf("[APP] no key within %lus, back to main menu",
         static_cast<unsigned long>(Config::CHOOSE_MENU_TIMEOUT_MS / 1000UL));
    enter(AppState::MAIN_MENU, now);
    return;
  }

  switch (in.key) {
    case '8':  // down
      if (selectedIndex + 1 < count) ++selectedIndex;
      redraw = true;
      break;
    case '2':  // up
      if (selectedIndex > 0) --selectedIndex;
      redraw = true;
      break;
    default:
      break;
  }

  // Keep the selection on screen.
  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex >= scrollOffset + VISIBLE_MENU_ROWS) scrollOffset = selectedIndex - VISIBLE_MENU_ROWS + 1;

  if (in.enter) {
    setWaste(static_cast<int>(selectedIndex));
    logf("[APP] waste type: %s", wasteLabel());
    enter(AppState::WEIGHING, now);
    return;
  }

  if (redraw) {
    drawWasteList(selectedIndex, scrollOffset);
    markDrawn(now);
  }
}

void runWeighing(const InputEvents &in, uint32_t now) {
  if (in.back) {
    enter(AppState::CHOOSE_MENU, now);
    return;
  }

  const ScaleSnapshot scale = scaleSnapshot(now);

  if (in.send) {
    // Refused locally, before anything is sent.
    const char *refusal = nullptr;
    if (!scale.live) refusal = "Timbangan N/A";
    else if (!std::isfinite(scale.weightKg) || scale.weightKg < Config::MIN_SEND_WEIGHT_KG)
      refusal = "Berat tidak valid";
    else if (!wifiConnected()) refusal = "WiFi terputus";

    if (!refusal) {
      pendingSeq = apiRequestSend(static_cast<size_t>(wasteIndex), scale.weightKg);
      if (pendingSeq == 0) refusal = "Sistem sibuk";
    }
    if (refusal) {
      logf("[SEND] refused: %s (%.2fkg)", refusal, static_cast<double>(scale.weightKg));
      sendOk = false;
      snprintf(sendDetail, sizeof(sendDetail), "%s", refusal);
      enter(AppState::SEND_RESULT, now);
    } else {
      enter(AppState::SENDING, now);
    }
    return;
  }

  if (due(now, WEIGHING_REFRESH_MS)) {
    drawWeighing(wasteLabel(), scale.live, scale.weightKg, wifiConnected());
    markDrawn(now);
  }
}

void runSending(const ApiResult *result, uint32_t now) {
  if (result && result->seq == pendingSeq) {
    pendingSeq = 0;
    sendOk = result->send == SendVerdict::OK;
    if (sendOk) {
      sendDetail[0] = '\0';
    } else if (result->httpCode > 0) {
      snprintf(sendDetail, sizeof(sendDetail), "Kode: %d", result->httpCode);
    } else {
      snprintf(sendDetail, sizeof(sendDetail), "Tidak ada koneksi");
    }
    enter(AppState::SEND_RESULT, now);
    return;
  }
  if (now - stateEnteredMs >= API_WAIT_LIMIT_MS) {
    // The server may still have stored it: say so rather than "failed".
    logf("[SEND] no answer within %lus - check the server before resending",
         static_cast<unsigned long>(API_WAIT_LIMIT_MS / 1000UL));
    pendingSeq = 0;
    sendOk = false;
    snprintf(sendDetail, sizeof(sendDetail), "Timeout, cek data");
    enter(AppState::SEND_RESULT, now);
    return;
  }
  if (due(now, ANIMATION_MS)) {
    drawSending(animationFrame++);
    markDrawn(now);
  }
}

void runSendResult(uint32_t now) {
  if (now - stateEnteredMs >= Config::SEND_RESULT_SCREEN_MS) {
    enter(AppState::WEIGHING, now);
    return;
  }
  if (redraw) {
    drawSendResult(sendOk, sendDetail);
    markDrawn(now);
  }
}

}  // namespace

void beginApp() {
  drawBootScreen(Config::DEVICE_ID, Config::FIRMWARE_VERSION);
  enter(AppState::MAIN_MENU, millis());
}

void updateApp(uint32_t now) {
  const InputEvents in = pollInputs(now);

  // Results are always collected; one that no longer matches the pending
  // request is simply dropped.
  ApiResult resultStorage;
  const ApiResult *result = apiTakeResult(resultStorage) ? &resultStorage : nullptr;

  switch (state) {
    case AppState::MAIN_MENU: runMainMenu(in, now); break;
    case AppState::REGISTRATION: runRegistration(in, now); break;
    case AppState::CHOOSE_MENU: runChooseMenu(in, now); break;
    case AppState::WEIGHING: runWeighing(in, now); break;
    case AppState::SENDING: runSending(result, now); break;
    case AppState::SEND_RESULT: runSendResult(now); break;
  }
}

AppStatus appStatus() {
  AppStatus s;
  portENTER_CRITICAL(&statusMux);
  s.state = state;
  s.wasteIndex = wasteIndex;
  portEXIT_CRITICAL(&statusMux);
  s.stateName = stateName(s.state);
  return s;
}
