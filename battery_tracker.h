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
