// battery_tracker.h -- estimate days of battery runtime from SOC drop per wake.
//
// Remembers state across deep sleep (RTC memory) and counts timer wakes. Takes
// the SOC percentage from the EXISTING battery reader as input — it never reads
// voltage or changes the SOC math itself. See
// docs/.../reTerminal_E1001_BatteryTracker.
#pragma once
#include <Arduino.h>

// Call once per wake, AFTER WiFi teardown and >=10 s since wake. soc = battery
// percent from the existing reader; isButtonWake = green-button (EXT1) wake.
void battery_tracker_update(float soc, bool isButtonWake);

// Estimated days until empty, or -1.0 while still calibrating (fewer than 3
// timer cycles, or no measurable drop yet). sleepIntervalSec is the existing
// sleep/refresh interval (do not invent a new one).
float battery_tracker_get_days_remaining(uint32_t sleepIntervalSec);

uint32_t battery_tracker_get_cycles();        // timer wakes used for the math
uint32_t battery_tracker_get_button_wakes();  // button wakes (not used in math)

// ---------------------------------------------------------------------------
// Awake (no deep sleep) estimate: same drop-rate idea, but measured over
// wall-clock time instead of per-cycle, since an always-awake device has no
// timer cycles. Sample periodically while awake AND on battery.
// ---------------------------------------------------------------------------

// Feed one sample: soc% + battery mV, tagged with wall-clock epoch seconds. Pass
// epochNow = 0 (time not synced yet) to skip. Direction comes from the shared mV
// detector below; the estimate uses the SOC drop over time.
void battery_tracker_update_awake(float soc, int mv, uint32_t epochNow);

// Estimated days until empty from the awake samples, or -1.0 while calibrating
// (too little elapsed time, no measurable drop yet, or currently charging).
float battery_tracker_get_days_remaining_awake();

// ---------------------------------------------------------------------------
// Charge/discharge from battery VOLTAGE trend — shared by deep-sleep and awake
// modes (E1001 has no charge/VBUS pin; HWCDC isPlugged() is unreliable). Feed
// battery mV each awake sample AND each deep-sleep wake.
// ---------------------------------------------------------------------------

// Update the trend with a new mV reading. Returns true only on the sample that
// flips the state INTO charging (used to restart the discharge baseline).
bool battery_charging_update(int mv);

// True while the voltage trend says charging (rising mV).
bool battery_is_charging();
