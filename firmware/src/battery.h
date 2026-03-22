#pragma once
// ============================================================
// Battery voltage & percentage reading
// ============================================================

#include <Arduino.h>

struct BatteryData {
    float voltage;      // Volts (after divider correction)
    int   percentage;   // 0-100
};

/// Initialise the ADC and battery-enable pin.
void batteryInit();

/// Read battery voltage and compute percentage.
BatteryData batteryRead();
