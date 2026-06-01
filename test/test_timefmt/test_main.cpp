#include <unity.h>
#include "TimeFormat.h"

using namespace usage_monitor;

void test_now_for_zero_and_negative() {
  char b[16];
  umFormatCountdown(0, b, sizeof(b));   TEST_ASSERT_EQUAL_STRING("now", b);
  umFormatCountdown(-5, b, sizeof(b));  TEST_ASSERT_EQUAL_STRING("now", b);
}

void test_hours_minutes() {
  char b[16];
  umFormatCountdown(7980, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("2:13", b);   // 2h13m
}

void test_under_one_hour() {
  char b[16];
  umFormatCountdown(2700, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("0:45", b);   // 45m
}

void test_pad_minutes() {
  char b[16];
  umFormatCountdown(3660, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("1:01", b);   // 1h01m
}

void test_one_day_boundary() {
  char b[16];
  umFormatCountdown(86400, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("1d 0h", b);
}

void test_multi_day() {
  char b[16];
  umFormatCountdown(90000, b, sizeof(b)); TEST_ASSERT_EQUAL_STRING("1d 1h", b); // 25h
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_now_for_zero_and_negative);
  RUN_TEST(test_hours_minutes);
  RUN_TEST(test_under_one_hour);
  RUN_TEST(test_pad_minutes);
  RUN_TEST(test_one_day_boundary);
  RUN_TEST(test_multi_day);
  return UNITY_END();
}
