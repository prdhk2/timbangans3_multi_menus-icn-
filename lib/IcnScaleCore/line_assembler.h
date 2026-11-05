#pragma once

#include <cstddef>
#include <cstdint>

// Non-blocking line reader for a UART byte stream. Replaces
// Serial.readStringUntil('\n'), which blocks the loop for up to a second on a
// partial line and allocates a String per frame. Pure C++, unit-tested
// natively in test/test_line_assembler/.
//
// Feed bytes one at a time; push() returns true when a complete, non-empty
// line is ready in line(). '\r' is ignored, so "\r\n" and "\n" both work. A
// line longer than Capacity is dropped whole - including the rest of it up to
// the next '\n' - rather than being cut into two garbage frames.
template <size_t Capacity>
class LineAssembler {
 public:
  bool push(char c) {
    if (c == '\n') {
      bool wasDiscarding = discarding_;
      discarding_ = false;
      size_t len = len_;
      len_ = 0;
      if (wasDiscarding || len == 0) return false;
      buffer_[len] = '\0';
      return true;
    }
    if (c == '\r' || c == '\0' || discarding_) return false;
    if (len_ >= Capacity) {
      discarding_ = true;
      len_ = 0;
      ++overflows_;
      return false;
    }
    buffer_[len_++] = c;
    return false;
  }

  // Valid after push() returned true, until the next push().
  const char *line() const { return buffer_; }

  uint32_t overflows() const { return overflows_; }

 private:
  char buffer_[Capacity + 1] = {};
  size_t len_ = 0;
  bool discarding_ = false;
  uint32_t overflows_ = 0;
};
