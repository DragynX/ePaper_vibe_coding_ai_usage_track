#include <unity.h>
#include <time.h>
#include <string.h>
#include "IsoTime.h"

using namespace usage_monitor;

// Build a known UTC epoch via timegm for comparison.
static long utcEpoch(int y, int mo, int d, int h, int mi, int s) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon  = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min  = mi;
  t.tm_sec  = s;
  return static_cast<long>(timegm(&t));
}

void test_iso_z() {
  TEST_ASSERT_EQUAL_INT(utcEpoch(2026, 5, 30, 12, 0, 0),
                        umParseIso8601("2026-05-30T12:00:00Z"));
}

void test_iso_fractional_seconds_ignored() {
  TEST_ASSERT_EQUAL_INT(umParseIso8601("2026-05-30T12:00:00Z"),
                        umParseIso8601("2026-05-30T12:00:00.123Z"));
}

void test_iso_positive_offset() {
  // +08:00 means local is 8h ahead of UTC -> UTC epoch is 8h earlier than Z.
  TEST_ASSERT_EQUAL_INT(umParseIso8601("2026-05-30T12:00:00Z") - 8 * 3600,
                        umParseIso8601("2026-05-30T12:00:00+08:00"));
}

void test_iso_space_separator() {
  TEST_ASSERT_EQUAL_INT(utcEpoch(2026, 5, 30, 12, 0, 0),
                        umParseIso8601("2026-05-30 12:00:00"));
}

void test_iso_bad_input() {
  TEST_ASSERT_EQUAL_INT(0, umParseIso8601("not a date"));
  TEST_ASSERT_EQUAL_INT(0, umParseIso8601(nullptr));
}

void test_normalize_reset() {
  const long now = 1748000000L;
  TEST_ASSERT_EQUAL_INT(1748600000L, umNormalizeReset(1748600000L, 0, now));      // absolute
  TEST_ASSERT_EQUAL_INT(now + 3600,  umNormalizeReset(0, 3600, now));             // relative
  TEST_ASSERT_EQUAL_INT(1748600000L, umNormalizeReset(1748600000L, 3600, now));   // both -> absolute wins
  TEST_ASSERT_EQUAL_INT(0,           umNormalizeReset(0, 0, now));                // none
}

void test_claude_token_expired() {
  const long now = 1748000000L;
  TEST_ASSERT_TRUE(umClaudeTokenExpired(0, now));            // missing
  TEST_ASSERT_TRUE(umClaudeTokenExpired(now + 299, now));    // within 5-min buffer
  TEST_ASSERT_FALSE(umClaudeTokenExpired(now + 1000, now));  // comfortably valid
}

void test_codex_token_expired() {
  const long now = 1748000000L;
  TEST_ASSERT_TRUE(umCodexTokenExpired(0, now));                       // missing
  TEST_ASSERT_TRUE(umCodexTokenExpired(now - 9L * 24 * 3600, now));    // 9 days old
  TEST_ASSERT_FALSE(umCodexTokenExpired(now - 7L * 24 * 3600, now));   // 7 days old
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_iso_z);
  RUN_TEST(test_iso_fractional_seconds_ignored);
  RUN_TEST(test_iso_positive_offset);
  RUN_TEST(test_iso_space_separator);
  RUN_TEST(test_iso_bad_input);
  RUN_TEST(test_normalize_reset);
  RUN_TEST(test_claude_token_expired);
  RUN_TEST(test_codex_token_expired);
  return UNITY_END();
}
