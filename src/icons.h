#pragma once
// ============================================================
// Icon drawing — procedural icons for the 7-color ePaper
// ============================================================
//
// All icons are drawn using GxEPD2 / Adafruit_GFX primitives so the
// firmware works without running the optional convert_icons.py script.
// When icon_data.h is generated (HAVE_ICON_DATA), bitmap versions are
// used instead for richer visuals.
// ============================================================

#include <GxEPD2_7c.h>
#include <epd7c/GxEPD2_730c_GDEP073E01.h>

// Forward-declare the display type used everywhere
typedef GxEPD2_7C<GxEPD2_730c_GDEP073E01,
                  GxEPD2_730c_GDEP073E01::HEIGHT> Display_t;

// ---- Colour aliases ----
#define COL_BLACK   GxEPD_BLACK
#define COL_WHITE   GxEPD_WHITE
#define COL_GREEN   GxEPD_GREEN
#define COL_BLUE    GxEPD_BLUE
#define COL_RED     GxEPD_RED
#define COL_YELLOW  GxEPD_YELLOW
#define COL_ORANGE  GxEPD_ORANGE

// ---- Weather condition icons (large, ~60×60) ----

/// Draw a weather icon by met.no symbol_code at (x, y) with given size.
void iconDrawWeather(Display_t &dsp, int16_t x, int16_t y, int16_t size,
                     const char *symbolCode);

/// Draw a small weather icon (for forecast row) at (x, y), size ~36.
void iconDrawWeatherSmall(Display_t &dsp, int16_t x, int16_t y,
                          const char *symbolCode);

// ---- Info icons (small, ~22×22) ----

/// Draw a sunrise icon (half-sun above horizon).
void iconDrawSunrise(Display_t &dsp, int16_t x, int16_t y, int16_t sz);

/// Draw a sunset icon (half-sun below horizon).
void iconDrawSunset(Display_t &dsp, int16_t x, int16_t y, int16_t sz);

/// Draw a humidity water-droplet icon.
void iconDrawHumidity(Display_t &dsp, int16_t x, int16_t y, int16_t sz);

/// Draw a barometric pressure gauge icon.
void iconDrawPressure(Display_t &dsp, int16_t x, int16_t y, int16_t sz);

/// Draw a rain-probability cloud + drop icon.
void iconDrawRainProb(Display_t &dsp, int16_t x, int16_t y, int16_t sz);

/// Draw a wind direction arrow rotated to \p angleDeg (0=N).
void iconDrawWindArrow(Display_t &dsp, int16_t cx, int16_t cy, int16_t sz,
                       float angleDeg);

/// Draw an Air Quality indicator circle with check/exclamation/close.
/// \p aqi 1-5 (1-2 green, 3-4 orange, 5 red).
void iconDrawAQI(Display_t &dsp, int16_t cx, int16_t cy, int16_t r, int aqi);

// ---- UI icons ----

/// Draw a battery icon with fill percentage at (x, y), w×h.
void iconDrawBattery(Display_t &dsp, int16_t x, int16_t y,
                     int16_t w, int16_t h, int pct);

/// Draw a circular "refresh" arrow icon (mdiRestore style).
void iconDrawRefresh(Display_t &dsp, int16_t cx, int16_t cy, int16_t r);
