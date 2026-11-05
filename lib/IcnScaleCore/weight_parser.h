#pragma once

// Weight from one line of the scale indicator's serial output. Pure C++,
// unit-tested natively in test/test_weight_parser/.
//
// Indicators prefix and suffix the number with status fields and units, e.g.
//   "ST,GS,+  12.34kg"   "   12.34 kg"   "US,NT,-0001.20kg"
// The first number on the line is the weight: an optional '-' before it, then
// digits with at most one '.', ending at the first other character. Spaces
// between the '-' and the digits are allowed. This is the rule the original
// firmware's extractNumericPart() applied, now with the failure cases (no
// digits, a lone '-', absurd magnitude) reported instead of read as 0.
namespace WeightParser {

// Anything beyond this is a corrupted line, not a reading.
constexpr float MAX_ABS_WEIGHT = 1.0e6f;

bool parse(const char *line, float &weightOut);

}  // namespace WeightParser
