#pragma once

// Convert the measured ADC millivolts (the DIVIDED battery voltage) into a
// 0-100 percentage. Hardware halves the real voltage, so multiply by 2.
// Maps 3.3 V -> 0 %, fullMv -> 100 % (default 4200 mV), clamped.
inline int umBatteryPercent(int milliVolts, int fullMv = 4200)
{
    const float v = (milliVolts / 1000.0f) * 2.0f;
    const float fullV = fullMv / 1000.0f;
    const float span = (fullV - 3.3f) > 0.01f ? (fullV - 3.3f) : 0.9f;
    float pct = (v - 3.3f) / span * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return static_cast<int>(pct + 0.5f);
}
