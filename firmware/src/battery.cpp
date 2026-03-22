// ============================================================
// Battery voltage & percentage reading
// ============================================================

#include "battery.h"
#include "config.h"

static const char *TAG = "Battery";

void batteryInit() {
    pinMode(BAT_EN, OUTPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC, ADC_11db);
    Serial.printf("[%s] Initialised (ADC pin %d, enable pin %d)\n", TAG, BAT_ADC, BAT_EN);
}

BatteryData batteryRead() {
    // Enable the battery monitoring circuit
    digitalWrite(BAT_EN, HIGH);
    delay(10);  // Seeed recommends 10 ms settle time

    // Take several samples and average for better accuracy
    const int SAMPLES = 16;
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogReadMilliVolts(BAT_ADC);
        delayMicroseconds(500);
    }

    // Disable battery monitoring to save power
    digitalWrite(BAT_EN, LOW);

    float measuredMv = (float)sum / SAMPLES;
    // x2 voltage divider on the board
    float voltage = (measuredMv / 1000.0f) * 2.0f;

    // Clamp to valid range
    if (voltage > BAT_VOLTAGE_MAX) voltage = BAT_VOLTAGE_MAX;
    if (voltage < BAT_VOLTAGE_MIN) voltage = BAT_VOLTAGE_MIN;

    // Linear interpolation for percentage (good enough for LiPo dashboard use)
    int pct = (int)(((voltage - BAT_VOLTAGE_MIN) / (BAT_VOLTAGE_MAX - BAT_VOLTAGE_MIN)) * 100.0f);
    if (pct > 100) pct = 100;
    if (pct < 0)   pct = 0;

    Serial.printf("[%s] Voltage: %.2f V  (%d%%)\n", TAG, voltage, pct);

    return {voltage, pct};
}
