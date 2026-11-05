#include "weight_parser.h"

#include <cmath>
#include <cstdlib>

namespace WeightParser {

namespace {

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

bool parse(const char *line, float &weightOut) {
  if (!line) return false;

  // Copy the numeric token into a small buffer: sign, digits, one dot.
  char token[24];
  size_t len = 0;
  bool negative = false;
  bool seenDigit = false;
  bool seenDot = false;

  for (const char *p = line; *p; ++p) {
    const char c = *p;
    if (isDigit(c)) {
      if (len + 1 >= sizeof(token)) return false;  // absurdly long number
      token[len++] = c;
      seenDigit = true;
    } else if (c == '.' && seenDigit && !seenDot) {
      if (len + 1 >= sizeof(token)) return false;
      token[len++] = c;
      seenDot = true;
    } else if (seenDigit) {
      break;  // the number ended
    } else if (c == '-') {
      negative = true;  // only counts before the first digit
    } else if (c != ' ' && c != '\t' && c != '+') {
      negative = false;  // a '-' separated from the digits by text is not a sign
    }
  }
  if (!seenDigit) return false;
  token[len] = '\0';

  const float value = strtof(token, nullptr);
  if (!std::isfinite(value) || value > MAX_ABS_WEIGHT) return false;
  weightOut = negative ? -value : value;
  return true;
}

}  // namespace WeightParser
