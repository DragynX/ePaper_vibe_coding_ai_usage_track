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
