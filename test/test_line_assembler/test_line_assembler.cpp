#include <unity.h>

#include "line_assembler.h"

void setUp() {}
void tearDown() {}

template <size_t N>
bool feed(LineAssembler<N> &a, const char *text) {
  bool ready = false;
  for (const char *p = text; *p; ++p) ready = a.push(*p);
  return ready;
}

void test_line_completes_on_newline() {
  LineAssembler<16> a;
  TEST_ASSERT_FALSE(feed(a, "1,2,3"));
  TEST_ASSERT_TRUE(a.push('\n'));
  TEST_ASSERT_EQUAL_STRING("1,2,3", a.line());
}

void test_crlf_is_stripped() {
  LineAssembler<16> a;
  TEST_ASSERT_TRUE(feed(a, "abc\r\n"));
  TEST_ASSERT_EQUAL_STRING("abc", a.line());
}

void test_empty_lines_are_ignored() {
  LineAssembler<16> a;
  TEST_ASSERT_FALSE(a.push('\n'));
  TEST_ASSERT_FALSE(feed(a, "\r\n"));
}

void test_consecutive_lines() {
  LineAssembler<16> a;
  TEST_ASSERT_TRUE(feed(a, "one\n"));
  TEST_ASSERT_EQUAL_STRING("one", a.line());
  TEST_ASSERT_TRUE(feed(a, "two\n"));
  TEST_ASSERT_EQUAL_STRING("two", a.line());
}

void test_line_at_exact_capacity_is_kept() {
  LineAssembler<4> a;
  TEST_ASSERT_TRUE(feed(a, "abcd\n"));
  TEST_ASSERT_EQUAL_STRING("abcd", a.line());
  TEST_ASSERT_EQUAL_UINT32(0, a.overflows());
}

void test_overlong_line_dropped_whole() {
  LineAssembler<4> a;
  // The tail after the overflow must not surface as a line of its own.
  TEST_ASSERT_FALSE(feed(a, "abcdefgh\n"));
  TEST_ASSERT_EQUAL_UINT32(1, a.overflows());
  // And the reader recovers on the next line.
  TEST_ASSERT_TRUE(feed(a, "ok\n"));
  TEST_ASSERT_EQUAL_STRING("ok", a.line());
}

void test_nul_bytes_ignored() {
  LineAssembler<8> a;
  a.push('a');
  a.push('\0');
  a.push('b');
  TEST_ASSERT_TRUE(a.push('\n'));
  TEST_ASSERT_EQUAL_STRING("ab", a.line());
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_line_completes_on_newline);
  RUN_TEST(test_crlf_is_stripped);
  RUN_TEST(test_empty_lines_are_ignored);
  RUN_TEST(test_consecutive_lines);
  RUN_TEST(test_line_at_exact_capacity_is_kept);
  RUN_TEST(test_overlong_line_dropped_whole);
  RUN_TEST(test_nul_bytes_ignored);
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
