#include <unity.h>
#include <math.h>
#include "QuotaMath.h"
#include "HeaderField.h"
#include "UsageSnapshot.h"

using namespace usage_monitor;

static void assertClose(double a, double b) { TEST_ASSERT_TRUE(fabs(a - b) < 1e-6); }

// ---- QuotaMath ----
void test_clamp_percent() {
  assertClose(0.0,   umClampPercent(-5.0));
  assertClose(100.0, umClampPercent(120.0));
  assertClose(42.5,  umClampPercent(42.5));
}

void test_remaining_percent() {
  assertClose(70.0, umRemainingPercent(30.0));
  assertClose(0.0,  umRemainingPercent(120.0));   // clamp then 100 - 100
  assertClose(100.0, umRemainingPercent(-5.0));    // clamp then 100 - 0
}

void test_status_from_remaining_thresholds() {
  TEST_ASSERT_TRUE(umStatusFromRemaining(0.0)   == QuotaStatus::kDepleted);
  TEST_ASSERT_TRUE(umStatusFromRemaining(19.0)  == QuotaStatus::kCritical);
  TEST_ASSERT_TRUE(umStatusFromRemaining(20.0)  == QuotaStatus::kWarning);
  TEST_ASSERT_TRUE(umStatusFromRemaining(49.0)  == QuotaStatus::kWarning);
  TEST_ASSERT_TRUE(umStatusFromRemaining(50.0)  == QuotaStatus::kHealthy);
  TEST_ASSERT_TRUE(umStatusFromRemaining(100.0) == QuotaStatus::kHealthy);
}

void test_status_from_used() {
  TEST_ASSERT_TRUE(umStatusFromUsed(100.0) == QuotaStatus::kDepleted);
  TEST_ASSERT_TRUE(umStatusFromUsed(120.0) == QuotaStatus::kDepleted);  // clamp
  TEST_ASSERT_TRUE(umStatusFromUsed(0.0)   == QuotaStatus::kHealthy);
}

// ---- HeaderField ----
void test_parse_double() {
  double v = 0.0;
  TEST_ASSERT_TRUE(umParseDouble("40", v));   assertClose(40.0, v);
  TEST_ASSERT_TRUE(umParseDouble("42.5", v)); assertClose(42.5, v);
  TEST_ASSERT_FALSE(umParseDouble("", v));
  TEST_ASSERT_FALSE(umParseDouble("abc", v));
  TEST_ASSERT_FALSE(umParseDouble(nullptr, v));
}

void test_pick_number() {
  bool present = false;
  assertClose(40.0, umPickNumber("40", 10.0, true, present));  TEST_ASSERT_TRUE(present);
  assertClose(55.0, umPickNumber("", 55.0, true, present));    TEST_ASSERT_TRUE(present);
  assertClose(10.0, umPickNumber("abc", 10.0, true, present)); TEST_ASSERT_TRUE(present);  // malformed header -> body
  umPickNumber("", 0.0, false, present);                       TEST_ASSERT_FALSE(present); // nothing available
}

// ---- UsageSnapshot ----
void test_window_remaining() {
  WindowQuota w;
  w.usedPercent = 30.0;
  assertClose(70.0, w.remainingPercent());
}

void test_is_stale() {
  ProviderQuota p;
  p.ok = true;
  p.lastSuccessEpoch = 1000;
  TEST_ASSERT_FALSE(p.isStale(1000 + 600, 900));   // 10 min < 15 min ttl
  TEST_ASSERT_TRUE(p.isStale(1000 + 1200, 900));   // 20 min > 15 min ttl

  p.ok = false;
  TEST_ASSERT_TRUE(p.isStale(1000, 900));          // last fetch failed

  ProviderQuota fresh;                             // never succeeded
  TEST_ASSERT_TRUE(fresh.isStale(1000, 900));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_clamp_percent);
  RUN_TEST(test_remaining_percent);
  RUN_TEST(test_status_from_remaining_thresholds);
  RUN_TEST(test_status_from_used);
  RUN_TEST(test_parse_double);
  RUN_TEST(test_pick_number);
  RUN_TEST(test_window_remaining);
  RUN_TEST(test_is_stale);
  return UNITY_END();
}
