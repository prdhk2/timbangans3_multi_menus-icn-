#include <unity.h>

#include "weight_parser.h"

using WeightParser::parse;

void setUp() {}
void tearDown() {}

void test_plain_and_unit_suffixed_lines() {
  float kg = 0;
  TEST_ASSERT_TRUE(parse("12.34", kg));
  TEST_ASSERT_EQUAL_FLOAT(12.34f, kg);
  TEST_ASSERT_TRUE(parse("   12.34 kg", kg));
  TEST_ASSERT_EQUAL_FLOAT(12.34f, kg);
  TEST_ASSERT_TRUE(parse("0.5kg\r", kg));
  TEST_ASSERT_EQUAL_FLOAT(0.5f, kg);
}

void test_indicator_status_prefixes() {
  float kg = 0;
  TEST_ASSERT_TRUE(parse("ST,GS,+  12.34kg", kg));
  TEST_ASSERT_EQUAL_FLOAT(12.34f, kg);
  TEST_ASSERT_TRUE(parse("US,NT,-0001.20kg", kg));
  TEST_ASSERT_EQUAL_FLOAT(-1.2f, kg);
  TEST_ASSERT_TRUE(parse("ST,GS,-  3.50kg", kg));  // spaces between sign and digits
  TEST_ASSERT_EQUAL_FLOAT(-3.5f, kg);
}

void test_first_number_wins() {
  float kg = 0;
  TEST_ASSERT_TRUE(parse("12.3 45.6", kg));
  TEST_ASSERT_EQUAL_FLOAT(12.3f, kg);
  TEST_ASSERT_TRUE(parse("1.2.3", kg));  // only one decimal point
  TEST_ASSERT_EQUAL_FLOAT(1.2f, kg);
}

void test_dash_separated_by_text_is_not_a_sign() {
  float kg = 0;
  TEST_ASSERT_TRUE(parse("-X 5", kg));
  TEST_ASSERT_EQUAL_FLOAT(5.0f, kg);
}

void test_rejects_lines_without_a_number() {
  float kg = 42;
  TEST_ASSERT_FALSE(parse("", kg));
  TEST_ASSERT_FALSE(parse("ST,GS", kg));
  TEST_ASSERT_FALSE(parse("-", kg));
  TEST_ASSERT_FALSE(parse(".", kg));
  TEST_ASSERT_FALSE(parse(nullptr, kg));
  TEST_ASSERT_EQUAL_FLOAT(42.0f, kg);  // untouched on failure
}

void test_rejects_absurd_values() {
  float kg = 0;
  TEST_ASSERT_FALSE(parse("99999999999", kg));
  TEST_ASSERT_FALSE(parse("123456789012345678901234567890", kg));
  TEST_ASSERT_TRUE(parse("1000000", kg));  // bound inclusive
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_plain_and_unit_suffixed_lines);
  RUN_TEST(test_indicator_status_prefixes);
  RUN_TEST(test_first_number_wins);
  RUN_TEST(test_dash_separated_by_text_is_not_a_sign);
  RUN_TEST(test_rejects_lines_without_a_number);
  RUN_TEST(test_rejects_absurd_values);
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
  delay(2000);  // let the serial monitor attach
  runAllTests();
}
void loop() {}
#else
int main() { return runAllTests(); }
#endif
