// ============================================================
// Display renderer — full dashboard layout for 800×480 7-color ePaper
// ============================================================

#include "display_renderer.h"
#include "time_utils.h"
#include "config.h"
#include <SPI.h>

// Adafruit GFX fonts
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

static const char *TAG = "Display";

// ---- Utility ----

/// Convert temperature based on user preference.
static float convTemp(float c, bool celsius) {
    return celsius ? c : (c * 9.0f / 5.0f + 32.0f);
}

/// Temperature unit string.
static const char *tempUnit(bool celsius) { return celsius ? "C" : "F"; }

/// Convert wind speed from m/s to the requested unit.
static float convWind(float ms, const char *unit) {
    if (strcmp(unit, "mph") == 0)   return ms * 2.23694f;
    if (strcmp(unit, "m/s") == 0)   return ms;
    if (strcmp(unit, "knots") == 0) return ms * 1.94384f;
    /* default km/h */             return ms * 3.6f;
}

/// Right-align text ending at xRight, baseline y.
static void drawTextRight(Display_t &dsp, const char *text,
                           int16_t xRight, int16_t y) {
    int16_t x1, y1;
    uint16_t tw, th;
    dsp.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
    dsp.setCursor(xRight - (int)tw, y);
    dsp.print(text);
}

/// Center text horizontally in a column [xLeft, xLeft+w], baseline y.
static void drawTextCenter(Display_t &dsp, const char *text,
                            int16_t xLeft, int16_t w, int16_t y) {
    int16_t x1, y1;
    uint16_t tw, th;
    dsp.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
    dsp.setCursor(xLeft + (w - (int)tw) / 2, y);
    dsp.print(text);
}

// ---- Event dot colour mapping ----
static const uint16_t DOT_COLORS[] = {
    COL_GREEN, COL_BLUE, COL_RED, COL_ORANGE, COL_YELLOW
};

// ================================================================
// HEADER
// ================================================================

static void drawHeader(Display_t &dsp, const DashboardData &d) {
    // 1.1 Date — left aligned
    String dateStr = timeFormatDate(d.now);
    dsp.setFont(&FreeSansBold18pt7b);
    dsp.setTextColor(COL_BLACK);
    dsp.setCursor(12, 38);
    dsp.print(dateStr);

    // 1.2 Battery — right top
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", d.battery.percentage);
        dsp.setFont(&FreeSans9pt7b);
        dsp.setTextColor(COL_BLACK);
        drawTextRight(dsp, buf, 755, 20);
        iconDrawBattery(dsp, 760, 8, 30, 15, d.battery.percentage);
    }

    // 1.3 Refresh time — right bottom
    {
        String hm = timeFormatHM(d.now);
        dsp.setFont(&FreeSans9pt7b);
        dsp.setTextColor(COL_BLACK);
        drawTextRight(dsp, hm.c_str(), 755, 47);
        iconDrawRefresh(dsp, 775, 42, 8);
    }

    // Separator line
    dsp.drawLine(0, HEADER_SEP_Y, DISPLAY_WIDTH - 1, HEADER_SEP_Y, COL_BLACK);
}

// ================================================================
// WEATHER COLUMN (left)
// ================================================================

// ---- 2.1 Current weather ----

static void drawCurrentWeather(Display_t &dsp, const DashboardData &d) {
    const WeatherData &w = d.weather;

    // Weather icon (large)
    iconDrawWeather(dsp, WX_CUR_ICON_X, WX_CUR_ICON_Y, WX_CUR_ICON_SZ,
                    w.symbolCode);

    // Current temperature (big)
    float temp = convTemp(w.currentTemp, d.useCelsius);
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);

    dsp.setFont(&FreeSansBold24pt7b);
    dsp.setTextColor(COL_BLACK);
    dsp.setCursor(110, WX_CUR_Y + 80);
    dsp.print(tempBuf);

    // Degree + unit (smaller, top-aligned to the right)
    int16_t x1, y1;
    uint16_t tw, th;
    dsp.getTextBounds(tempBuf, 110, WX_CUR_Y + 80, &x1, &y1, &tw, &th);
    int unitX = 110 + (int)tw + 2;

    char unitBuf[8];
    snprintf(unitBuf, sizeof(unitBuf), "°%s", tempUnit(d.useCelsius));
    dsp.setFont(&FreeSans12pt7b);
    dsp.setCursor(unitX, WX_CUR_Y + 50);
    dsp.print(unitBuf);

    // Min / Max temperatures
    float minT = convTemp(w.minTemp, d.useCelsius);
    float maxT = convTemp(w.maxTemp, d.useCelsius);
    char minBuf[16], maxBuf[16];
    snprintf(minBuf, sizeof(minBuf), "%.0f°", minT);
    snprintf(maxBuf, sizeof(maxBuf), "%.0f°", maxT);

    dsp.setFont(&FreeSans9pt7b);
    // Min (blue) — draw a small down-arrow manually
    dsp.setTextColor(COL_BLUE);
    dsp.fillTriangle(117, WX_CUR_Y + 105, 121, WX_CUR_Y + 112, 113, WX_CUR_Y + 112, COL_BLUE);
    dsp.setCursor(125, WX_CUR_Y + 115);
    dsp.print(minBuf);

    // Max (red) — draw a small up-arrow manually
    dsp.setTextColor(COL_RED);
    dsp.fillTriangle(187, WX_CUR_Y + 112, 191, WX_CUR_Y + 105, 183, WX_CUR_Y + 105, COL_RED);
    dsp.setCursor(195, WX_CUR_Y + 115);
    dsp.print(maxBuf);

    dsp.setTextColor(COL_BLACK);
}

// ---- 2.2 Today's info grid ----

static void drawInfoGrid(Display_t &dsp, const DashboardData &d) {
    const WeatherData &w = d.weather;

    int baseY = WX_INFO_Y;
    int cw = WX_INFO_CW;
    int rh = WX_INFO_RH;

    struct InfoCell {
        int col, row;
        void (*drawIcon)(Display_t &dsp, const DashboardData &d,
                         int16_t ix, int16_t iy, int16_t isz);
        const char *title;
        char value[32];
    };

    // Determine sunrise/sunset: show next event
    bool showSunset = false;
    {
        int nowMin = d.now.tm_hour * 60 + d.now.tm_min;
        int riseMin = w.sunriseH * 60 + w.sunriseM;
        int setMin  = w.sunsetH  * 60 + w.sunsetM;
        // If current time is after sunrise but before sunset → show sunset
        if (nowMin >= riseMin && nowMin < setMin) showSunset = true;
    }

    // Build cell data
    struct {
        int col, row;
        const char *title;
        char value[32];
    } cells[6];

    // [0,0] Sunrise/Sunset
    cells[0].col = 0; cells[0].row = 0;
    if (showSunset) {
        cells[0].title = "Sunset";
        snprintf(cells[0].value, sizeof(cells[0].value), "%02d:%02d",
                 w.sunsetH, w.sunsetM);
    } else {
        cells[0].title = "Sunrise";
        snprintf(cells[0].value, sizeof(cells[0].value), "%02d:%02d",
                 w.sunriseH, w.sunriseM);
    }

    // [1,0] Air Quality
    cells[1].col = 1; cells[1].row = 0;
    cells[1].title = "Air Quality";
    snprintf(cells[1].value, sizeof(cells[1].value), "%d", w.aqi);

    // [2,0] Wind
    cells[2].col = 2; cells[2].row = 0;
    cells[2].title = "Wind";
    snprintf(cells[2].value, sizeof(cells[2].value), "%.0f %s",
             convWind(w.windSpeed, d.windUnit), d.windUnit);

    // [0,1] Precipitation (mm in next hour)
    cells[3].col = 0; cells[3].row = 1;
    cells[3].title = "Rain";
    snprintf(cells[3].value, sizeof(cells[3].value), "%.1f mm", w.precipitation);

    // [1,1] Pressure
    cells[4].col = 1; cells[4].row = 1;
    cells[4].title = "Pressure";
    snprintf(cells[4].value, sizeof(cells[4].value), "%.0f hPa", w.pressure);

    // [2,1] Humidity
    cells[5].col = 2; cells[5].row = 1;
    cells[5].title = "Humidity";
    snprintf(cells[5].value, sizeof(cells[5].value), "%.0f%%", w.humidity);

    for (int i = 0; i < 6; i++) {
        int cx = cells[i].col * cw;
        int cy = baseY + cells[i].row * rh;
        int iconX = cx + 8;
        int iconY = cy + 8;
        int iconSz = 24;

        // Draw icon
        switch (i) {
        case 0: // Sunrise / Sunset
            if (showSunset)
                iconDrawSunset(dsp, iconX, iconY, iconSz);
            else
                iconDrawSunrise(dsp, iconX, iconY, iconSz);
            break;
        case 1: // AQI
            iconDrawAQI(dsp, iconX + iconSz / 2, iconY + iconSz / 2,
                        iconSz / 2, w.aqi);
            break;
        case 2: // Wind
            iconDrawWindArrow(dsp, iconX + iconSz / 2, iconY + iconSz / 2,
                              iconSz, w.windDirection);
            break;
        case 3: // Rain
            iconDrawRainProb(dsp, iconX, iconY, iconSz);
            break;
        case 4: // Pressure
            iconDrawPressure(dsp, iconX, iconY, iconSz);
            break;
        case 5: // Humidity
            iconDrawHumidity(dsp, iconX, iconY, iconSz);
            break;
        }

        // Title (small, above value)
        int textX = cx + iconSz + 16;
        dsp.setFont(&FreeSans9pt7b);
        dsp.setTextColor(COL_BLACK);
        dsp.setCursor(textX, cy + 24);
        dsp.print(cells[i].title);

        // Value (larger)
        dsp.setFont(&FreeSansBold12pt7b);
        dsp.setCursor(textX, cy + rh - 18);
        dsp.print(cells[i].value);
    }
}

// ---- 2.3 Three-day forecast ----

static void drawForecast(Display_t &dsp, const DashboardData &d) {
    int baseY = WX_FCST_Y;
    int cw = WX_FCST_CW;

    // Thin separator above forecast
    dsp.drawLine(0, baseY - 3, WX_W - 1, baseY - 3, COL_BLACK);

    for (int i = 0; i < MAX_FORECAST_DAYS; i++) {
        const ForecastDay &fc = d.weather.forecast[i];
        int cx = i * cw;

        // Day name (bold, centered)
        dsp.setFont(&FreeSansBold9pt7b);
        dsp.setTextColor(COL_BLACK);
        drawTextCenter(dsp, fc.dayAbbr, cx, cw, baseY + 16);

        // Weather icon (small, centered)
        iconDrawWeatherSmall(dsp, cx + (cw - 36) / 2, baseY + 22, fc.symbolCode);

        // Max temp (red)
        char buf[12];
        float maxT = convTemp(fc.maxTemp, d.useCelsius);
        snprintf(buf, sizeof(buf), "%.0f°", maxT);
        dsp.setFont(&FreeSans9pt7b);
        dsp.setTextColor(COL_RED);
        drawTextCenter(dsp, buf, cx, cw, baseY + 72);

        // Min temp (blue)
        float minT = convTemp(fc.minTemp, d.useCelsius);
        snprintf(buf, sizeof(buf), "%.0f°", minT);
        dsp.setTextColor(COL_BLUE);
        drawTextCenter(dsp, buf, cx, cw, baseY + 90);
    }

    dsp.setTextColor(COL_BLACK);
}

// ================================================================
// CALENDAR COLUMN (right)
// ================================================================

// ---- 3.1 Calendar grid ----

static void drawCalendarGrid(Display_t &dsp, const DashboardData &d) {
    int year  = d.now.tm_year + 1900;
    int month = d.now.tm_mon + 1;
    int today = d.now.tm_mday;
    int daysInMonth = timeDaysInMonth(year, month);

    // Day-of-week for 1st of month (0=Sun). Convert to Mon-start: (dow+6)%7
    int firstDow = (timeDayOfWeek(year, month, 1) + 6) % 7;  // 0=Mon

    int x0 = CAL_X;
    int y0 = CAL_GRID_Y;
    int cellW = CAL_CW;
    int headerH = CAL_HEADER_H;

    // Number of rows needed
    int totalSlots = firstDow + daysInMonth;
    int numRows = (totalSlots + 6) / 7;
    int cellH = (CAL_GRID_H - headerH - 4) / numRows;

    // Header: Mon Tue Wed Thu Fri Sat Sun
    static const char *hdrs[] = {"Mo","Tu","We","Th","Fr","Sa","Su"};
    dsp.setFont(&FreeSansBold9pt7b);
    dsp.setTextColor(COL_BLACK);
    for (int c = 0; c < 7; c++) {
        drawTextCenter(dsp, hdrs[c], x0 + c * cellW, cellW, y0 + headerH - 4);
    }

    // Separator line under header
    dsp.drawLine(x0, y0 + headerH, x0 + CAL_W - 1, y0 + headerH, COL_BLACK);

    // Build a lookup: day → list of event color indices
    // Up to 4 colours per day
    uint8_t dayColors[32][4] = {};
    uint8_t dayColorCount[32] = {};
    for (int e = 0; e < d.calendar.eventCount; e++) {
        int eDay = d.calendar.events[e].day;
        if (eDay < 1 || eDay > 31) continue;
        if (dayColorCount[eDay] < 4) {
            dayColors[eDay][dayColorCount[eDay]] = d.calendar.events[e].colorIndex;
            dayColorCount[eDay]++;
        }
    }

    // Day numbers
    int gridY = y0 + headerH + 2;
    for (int dayNum = 1; dayNum <= daysInMonth; dayNum++) {
        int slot = firstDow + dayNum - 1;
        int col = slot % 7;
        int row = slot / 7;
        int cx = x0 + col * cellW + cellW / 2;
        int cy = gridY + row * cellH + cellH / 2;

        bool isToday   = (dayNum == today);
        bool isWeekend = (col >= 5);  // Sat(5), Sun(6)

        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d", dayNum);

        int16_t bx, by;
        uint16_t bw, bh;

        if (isToday) {
            // Red filled circle with white number
            int circR = min(cellW, cellH) / 2 - 3;
            if (circR < 10) circR = 10;
            dsp.fillCircle(cx, cy - 2, circR, COL_RED);
            dsp.setFont(&FreeSansBold9pt7b);
            dsp.setTextColor(COL_WHITE);
            dsp.getTextBounds(numBuf, 0, 0, &bx, &by, &bw, &bh);
            dsp.setCursor(cx - (int)bw / 2, cy + (int)bh / 2 - 2);
            dsp.print(numBuf);
            dsp.setTextColor(COL_BLACK);
        } else {
            // Regular day number
            if (isWeekend)
                dsp.setFont(&FreeSansBold9pt7b);
            else
                dsp.setFont(&FreeSans9pt7b);
            dsp.setTextColor(COL_BLACK);
            dsp.getTextBounds(numBuf, 0, 0, &bx, &by, &bw, &bh);
            dsp.setCursor(cx - (int)bw / 2, cy + (int)bh / 2 - 2);
            dsp.print(numBuf);

            // Event dots (up to 2×2 grid below the number)
            int dotR = 3;
            int nc = dayColorCount[dayNum];
            if (nc > 0) {
                int dotY = cy + (int)bh / 2 + 4;
                int spacing = dotR * 3;
                if (nc == 1) {
                    dsp.fillCircle(cx, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][0] % 5]);
                } else if (nc == 2) {
                    dsp.fillCircle(cx - spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][0] % 5]);
                    dsp.fillCircle(cx + spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][1] % 5]);
                } else if (nc == 3) {
                    dsp.fillCircle(cx - spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][0] % 5]);
                    dsp.fillCircle(cx + spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][1] % 5]);
                    dsp.fillCircle(cx, dotY + spacing, dotR,
                                   DOT_COLORS[dayColors[dayNum][2] % 5]);
                } else {
                    // 4 dots in 2×2 grid
                    dsp.fillCircle(cx - spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][0] % 5]);
                    dsp.fillCircle(cx + spacing / 2, dotY, dotR,
                                   DOT_COLORS[dayColors[dayNum][1] % 5]);
                    dsp.fillCircle(cx - spacing / 2, dotY + spacing, dotR,
                                   DOT_COLORS[dayColors[dayNum][2] % 5]);
                    dsp.fillCircle(cx + spacing / 2, dotY + spacing, dotR,
                                   DOT_COLORS[dayColors[dayNum][3] % 5]);
                }
            }
        }
    }
}

// ---- 3.2 Next event ----

static void drawNextEvent(Display_t &dsp, const DashboardData &d) {
    if (!d.calendar.ok || d.calendar.nextEventIdx < 0) {
        if (!d.calendar.ok && d.calendar.eventCount == 0) {
            // No calendar configured or no events
            dsp.setFont(&FreeSans9pt7b);
            dsp.setTextColor(COL_BLACK);
            dsp.setCursor(CAL_X + 10, CAL_EVENT_Y + 20);
            dsp.print("No upcoming events");
        }
        return;
    }

    int y0 = CAL_EVENT_Y;

    // Thin separator
    dsp.drawLine(CAL_X, y0 - 2, DISPLAY_WIDTH - 1, y0 - 2, COL_BLACK);

    const CalendarEvent &ev = d.calendar.events[d.calendar.nextEventIdx];

    // Start time
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ev.startHour, ev.startMin);
    dsp.setFont(&FreeSansBold12pt7b);
    dsp.setTextColor(COL_BLACK);
    dsp.setCursor(CAL_X + 12, y0 + 28);
    dsp.print(timeBuf);

    // Event title
    dsp.setFont(&FreeSans12pt7b);
    dsp.setCursor(CAL_X + 90, y0 + 28);
    // Truncate title if too long
    String title(ev.title);
    if (title.length() > 25) {
        title = title.substring(0, 23) + "..";
    }
    dsp.print(title);

    // Location (smaller, if available)
    if (strlen(ev.location) > 0) {
        dsp.setFont(&FreeSans9pt7b);
        dsp.setTextColor(COL_BLACK);
        dsp.setCursor(CAL_X + 90, y0 + 52);
        String loc(ev.location);
        if (loc.length() > 30) loc = loc.substring(0, 28) + "..";
        dsp.print(loc);
    }
}

// ================================================================
// PUBLIC API
// ================================================================

void displayInit(Display_t &dsp) {
    Serial.printf("[%s] Initialising display...\n", TAG);

    // Use HSPI bus (shared with SD card)
    static SPIClass hspi(HSPI);
    hspi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, -1);

    dsp.init(SERIAL_BAUD, true, 10, false, hspi,
             SPISettings(4000000, MSBFIRST, SPI_MODE0));
    dsp.setRotation(0);
    dsp.setFullWindow();
    dsp.setTextWrap(false);

    Serial.printf("[%s] Display ready (%dx%d)\n", TAG, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void displayRender(Display_t &dsp, const DashboardData &data) {
    Serial.printf("[%s] Rendering dashboard...\n", TAG);
    unsigned long t0 = millis();

    dsp.setFullWindow();
    dsp.firstPage();
    do {
        dsp.fillScreen(COL_WHITE);

        drawHeader(dsp, data);

        if (data.weather.weatherOk) {
            drawCurrentWeather(dsp, data);
            drawInfoGrid(dsp, data);
            drawForecast(dsp, data);
        } else {
            // Weather unavailable message
            dsp.setFont(&FreeSans12pt7b);
            dsp.setTextColor(COL_BLACK);
            dsp.setCursor(30, WX_CUR_Y + 60);
            dsp.print("Weather data");
            dsp.setCursor(30, WX_CUR_Y + 90);
            dsp.print("unavailable");
        }

        drawCalendarGrid(dsp, data);
        drawNextEvent(dsp, data);

    } while (dsp.nextPage());

    unsigned long elapsed = millis() - t0;
    Serial.printf("[%s] Render complete in %lu ms\n", TAG, elapsed);
}
