#include "peripherals.h"

#include "config.h"
#include "debug_log.h"
#include "line_assembler.h"
#include "weight_parser.h"

namespace {

// Per loop pass, so a noisy line can never starve the loop.
constexpr int MAX_BYTES_PER_PASS = 256;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

HardwareSerial &scalePort = Serial2;
LineAssembler<32> scaleLine;
float scaleWeight = 0.0f;
bool scaleEver = false;
uint32_t scaleLastMs = 0;
uint32_t scaleFrames = 0;
uint32_t scaleRejected = 0;
bool scaleAnnounced = false;

}  // namespace

void beginScale() {
  scalePort.begin(Config::SCALE_BAUD, SERIAL_8N1, Config::Pins::SCALE_RX, -1,
                  Config::SCALE_RX_INVERTED);
}

void updateScale(uint32_t now) {
  for (int i = 0; i < MAX_BYTES_PER_PASS && scalePort.available() > 0; ++i) {
    const int c = scalePort.read();
    if (c < 0) break;
    if (!scaleLine.push(static_cast<char>(c))) continue;

    // A line without a usable number is dropped, never read as NaN or 0.
    float kg = 0.0f;
    const bool ok = WeightParser::parse(scaleLine.line(), kg);
    portENTER_CRITICAL(&mux);
    if (ok) {
      scaleWeight = kg;
      scaleEver = true;
      scaleLastMs = now;
      ++scaleFrames;
    } else {
      ++scaleRejected;
    }
    portEXIT_CRITICAL(&mux);

    if (ok && !scaleAnnounced) {
      scaleAnnounced = true;
      logf("[SCALE] first reading: %.2f kg", static_cast<double>(kg));
    }
  }
}

ScaleSnapshot scaleSnapshot(uint32_t now) {
  ScaleSnapshot s;
  portENTER_CRITICAL(&mux);
  s.everReceived = scaleEver;
  s.weightKg = scaleWeight;
  s.ageMs = scaleEver ? now - scaleLastMs : UINT32_MAX;
  s.frames = scaleFrames;
  s.rejected = scaleRejected;
  portEXIT_CRITICAL(&mux);
  s.live = s.everReceived && s.ageMs < Config::SCALE_STALE_MS;
  return s;
}
