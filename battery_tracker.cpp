// battery_tracker.cpp -- see battery_tracker.h.
//
// All state lives in RTC memory so it survives deep sleep (normal RAM is wiped
// every wake). Button wakes are counted separately and excluded from the math:
// they are user-driven and irregular, so folding them into the per-cycle drop
// would corrupt the estimate.
#include "battery_tracker.h"

RTC_DATA_ATTR static bool     tracker_initialized = false;
RTC_DATA_ATTR static float    soc_at_start        = 0.0f;  // SOC% on the first wake
RTC_DATA_ATTR static float    soc_last            = 0.0f;  // SOC% from the last wake
RTC_DATA_ATTR static uint32_t total_cycles        = 0;     // timer wakes (drives math)
RTC_DATA_ATTR static uint32_t button_wake_count   = 0;     // button wakes (display only)

void battery_tracker_update(float soc, bool isButtonWake) {
  if (isButtonWake) {
    button_wake_count++;          // counted, but never used in the estimate
  } else {
    total_cycles++;               // a real sleep cycle drives the drop math
  }
  if (!tracker_initialized) {
    soc_at_start = soc;           // baseline SOC, set once
    tracker_initialized = true;
  }
  soc_last = soc;                 // latest SOC reading
}

float battery_tracker_get_days_remaining(uint32_t sleepIntervalSec) {
  if (total_cycles < 3) return -1.0f;            // early drop < ADC noise -> garbage
  const float consumed = soc_at_start - soc_last;
  if (consumed <= 0.0f) return -1.0f;            // no measurable drop yet
  const float dropPerCycle  = consumed / (float)total_cycles;
  const float cyclesLeft    = soc_last / dropPerCycle;
  return (cyclesLeft * (float)sleepIntervalSec) / 86400.0f;
}

uint32_t battery_tracker_get_cycles()       { return total_cycles; }
uint32_t battery_tracker_get_button_wakes() { return button_wake_count; }

// ---------------------------------------------------------------------------
// Awake (time-based) estimate. Always-awake mode has no timer cycles, so the
// drop is measured against wall-clock epoch seconds instead. State is RTC-backed
// so it survives the occasional reboot mid-session.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Charge/discharge direction from battery VOLTAGE movement, shared by both
// modes (E1001 has no charge/VBUS signal; HWCDC isPlugged() is unreliable).
// A pivot ratchets with the trend, so the state flips after ~one threshold of
// travel regardless of absolute level. Fed every awake sample AND every deep-
// sleep wake; RTC-backed so it persists across sleep.
// ---------------------------------------------------------------------------
static const int kRiseMv = 40;   // mV climb past pivot => charging (tunable)
static const int kDropMv = 40;   // mV fall  past pivot => discharging (tunable)

enum { DIR_CALIB = 0, DIR_DISCHARGE, DIR_CHARGE };

RTC_DATA_ATTR static bool   chg_init     = false;
RTC_DATA_ATTR static int    chg_pivot_mv = 0;
RTC_DATA_ATTR static int8_t chg_dir      = DIR_CALIB;

bool battery_charging_update(int mv) {
  if (!chg_init) { chg_pivot_mv = mv; chg_dir = DIR_CALIB; chg_init = true; return false; }
  const int d = mv - chg_pivot_mv;
  if (d >= kRiseMv) {
    const bool flipped = (chg_dir != DIR_CHARGE);
    chg_dir = DIR_CHARGE; chg_pivot_mv = mv;
    return flipped;                            // true only on the transition into charge
  } else if (-d >= kDropMv) {
    chg_dir = DIR_DISCHARGE; chg_pivot_mv = mv;
  }
  return false;                                // within band: hold (sticky)
}

bool battery_is_charging() { return chg_dir == DIR_CHARGE; }

// ---------------------------------------------------------------------------
// Awake (time-based) estimate. Direction comes from the shared mV detector.
// ---------------------------------------------------------------------------
static const uint32_t kAwakeMinElapsedSec = 900;    // need >=15 min before trusting
static const float    kAwakeMinDropPct    = 2.0f;   // and >=2% drop (ADC noise floor)

RTC_DATA_ATTR static bool     awake_init = false;
RTC_DATA_ATTR static float    awake_soc0 = 0.0f;  // estimate baseline SOC%
RTC_DATA_ATTR static uint32_t awake_t0   = 0;     // estimate baseline epoch (sec)
RTC_DATA_ATTR static float    awake_soc1 = 0.0f;  // latest SOC%
RTC_DATA_ATTR static uint32_t awake_t1   = 0;     // latest epoch (sec)

void battery_tracker_update_awake(float soc, int mv, uint32_t epochNow) {
  if (epochNow == 0) return;                  // time not valid yet -> skip
  if (!awake_init) {
    awake_soc0 = soc; awake_t0 = epochNow;
    awake_init = true;
  }
  awake_soc1 = soc; awake_t1 = epochNow;
  if (battery_charging_update(mv)) {          // flipped into charge ->
    awake_soc0 = soc; awake_t0 = epochNow;    // restart discharge baseline from the top
  }
}

float battery_tracker_get_days_remaining_awake() {
  if (!awake_init || battery_is_charging()) return -1.0f;  // no stale est while charging
  const uint32_t elapsed = (awake_t1 >= awake_t0) ? (awake_t1 - awake_t0) : 0;
  if (elapsed < kAwakeMinElapsedSec) return -1.0f;   // not enough time yet
  const float consumed = awake_soc0 - awake_soc1;
  if (consumed < kAwakeMinDropPct)   return -1.0f;   // no measurable drop yet
  const float ratePctPerSec = consumed / (float)elapsed;
  const float secsLeft      = awake_soc1 / ratePctPerSec;
  return secsLeft / 86400.0f;
}
