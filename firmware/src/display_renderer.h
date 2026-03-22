#pragma once
// ============================================================
// Display renderer — full dashboard layout for 800×480 7-color ePaper
// ============================================================

#include "icons.h"
#include "weather_api.h"
#include "calendar_api.h"
#include "battery.h"
#include <time.h>

/// Everything the renderer needs in one struct.
struct DashboardData {
    struct tm      now;
    BatteryData    battery;
    WeatherData    weather;
    CalendarData   calendar;
    bool           useCelsius;
    char           windUnit[8];    // "km/h", "mph", "m/s", "knots"
    float          tzOffsetHours;
};

/// Initialise the display hardware.
void displayInit(Display_t &dsp);

/// Render the complete dashboard to the e-paper.
void displayRender(Display_t &dsp, const DashboardData &data);
