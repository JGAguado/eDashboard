// ============================================================
// Icon drawing — procedural icons for 7-color ePaper
// ============================================================

#include "icons.h"
#include <math.h>

// ---- Internal helpers ----

static void drawSun(Display_t &dsp, int16_t cx, int16_t cy, int16_t r) {
    dsp.fillCircle(cx, cy, r, COL_YELLOW);
    // Rays
    for (int a = 0; a < 360; a += 45) {
        float rad = a * M_PI / 180.0f;
        int x1 = cx + (int)((r + 2) * sinf(rad));
        int y1 = cy - (int)((r + 2) * cosf(rad));
        int x2 = cx + (int)((r + 5) * sinf(rad));
        int y2 = cy - (int)((r + 5) * cosf(rad));
        dsp.drawLine(x1, y1, x2, y2, COL_YELLOW);
    }
}

static void drawCloud(Display_t &dsp, int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t color) {
    // Stylised cloud: overlapping circles + rectangle
    int r = h / 2;
    int cy = y + h - r;
    dsp.fillCircle(x + w / 3, y + r / 2, r, color);           // top bump
    dsp.fillCircle(x + r, cy, r, color);                       // left bump
    dsp.fillCircle(x + w - r, cy, r - 1, color);               // right bump
    dsp.fillRoundRect(x, cy - r / 2, w, r + r / 2, r / 2, color); // base
}

static void drawRainDrops(Display_t &dsp, int16_t x, int16_t y,
                           int16_t w, int count) {
    int spacing = w / (count + 1);
    for (int i = 0; i < count; i++) {
        int dx = x + spacing * (i + 1);
        int dy = y + (i % 2) * 4;
        dsp.fillCircle(dx, dy + 4, 2, COL_BLUE);
        dsp.drawLine(dx, dy, dx, dy + 3, COL_BLUE);
    }
}

static void drawSnowflakes(Display_t &dsp, int16_t x, int16_t y,
                            int16_t w, int count) {
    int spacing = w / (count + 1);
    for (int i = 0; i < count; i++) {
        int cx = x + spacing * (i + 1);
        int cy = y + 4 + (i % 2) * 3;
        dsp.drawPixel(cx, cy, COL_BLUE);
        dsp.drawPixel(cx - 1, cy - 1, COL_BLUE);
        dsp.drawPixel(cx + 1, cy - 1, COL_BLUE);
        dsp.drawPixel(cx - 1, cy + 1, COL_BLUE);
        dsp.drawPixel(cx + 1, cy + 1, COL_BLUE);
    }
}

static void drawLightning(Display_t &dsp, int16_t cx, int16_t y, int16_t h) {
    // Simple zigzag bolt
    int x0 = cx + 2, y0 = y;
    int x1 = cx - 2, y1 = y + h / 3;
    int x2 = cx + 1, y2 = y + h / 3;
    int x3 = cx - 3, y3 = y + 2 * h / 3;
    int x4 = cx,     y4 = y + 2 * h / 3;
    int x5 = cx - 4, y5 = y + h;
    dsp.drawLine(x0, y0, x1, y1, COL_YELLOW);
    dsp.drawLine(x1, y1, x2, y2, COL_YELLOW);
    dsp.drawLine(x2, y2, x3, y3, COL_YELLOW);
    dsp.drawLine(x3, y3, x4, y4, COL_YELLOW);
    dsp.drawLine(x4, y4, x5, y5, COL_YELLOW);
}

// ---- Weather icon by symbol_code ----

/// Determine the base weather type from a met.no symbol_code.
static int classifySymbol(const char *code) {
    // 0=clear, 1=fair, 2=partlycloudy, 3=cloudy, 4=fog,
    // 5=lightrain, 6=rain, 7=heavyrain, 8=thunder
    // 9=sleet, 10=snow, 11=lightsnow
    String s(code);
    if (s.startsWith("clearsky"))             return 0;
    if (s.startsWith("fair"))                 return 1;
    if (s.startsWith("partlycloudy"))         return 2;
    if (s.startsWith("cloudy"))               return 3;
    if (s.startsWith("fog"))                  return 4;
    if (s.indexOf("thunder") >= 0)            return 8;
    if (s.startsWith("heavyrain") ||
        s.startsWith("heavyrainshowers"))      return 7;
    if (s.startsWith("rain") ||
        s.startsWith("rainshowers") ||
        s.startsWith("lightrain") ||
        s.startsWith("lightrainshowers"))      return 6;
    if (s.indexOf("sleet") >= 0)              return 9;
    if (s.startsWith("heavysnow") ||
        s.startsWith("snow") ||
        s.startsWith("heavysnowshowers") ||
        s.startsWith("snowshowers"))           return 10;
    if (s.indexOf("snow") >= 0)               return 11;
    return 3;  // default: cloudy
}

static bool isNight(const char *code) {
    return strstr(code, "_night") != nullptr;
}

void iconDrawWeather(Display_t &dsp, int16_t x, int16_t y, int16_t sz,
                     const char *symbolCode) {
    int type = classifySymbol(symbolCode);
    bool night = isNight(symbolCode);

    int sunR = sz / 4;
    int cloudW = (int)(sz * 0.7f);
    int cloudH = (int)(sz * 0.35f);
    int cloudX = x + (sz - cloudW) / 2;
    int cloudY = y + sz / 3;

    switch (type) {
    case 0: // clear sky
        if (night) {
            // Moon crescent
            dsp.fillCircle(x + sz/2, y + sz/2, sunR + 2, COL_YELLOW);
            dsp.fillCircle(x + sz/2 + sunR/2, y + sz/2 - sunR/3, sunR + 1, COL_WHITE);
        } else {
            drawSun(dsp, x + sz/2, y + sz/2, sunR);
        }
        break;

    case 1: // fair (few clouds)
        if (!night) drawSun(dsp, x + sz/3, y + sz/4, sunR - 2);
        drawCloud(dsp, cloudX + 4, cloudY + 4, cloudW - 8, cloudH - 4, COL_BLACK);
        break;

    case 2: // partly cloudy
        if (!night) drawSun(dsp, x + sz/4, y + sz/5, sunR - 3);
        drawCloud(dsp, cloudX, cloudY, cloudW, cloudH, COL_BLACK);
        break;

    case 3: // cloudy
        drawCloud(dsp, cloudX, cloudY - 5, cloudW, cloudH, COL_BLACK);
        drawCloud(dsp, cloudX + 5, cloudY + 3, cloudW - 5, cloudH - 2, COL_BLACK);
        break;

    case 4: // fog
        drawCloud(dsp, cloudX, cloudY - 5, cloudW, cloudH - 4, COL_BLACK);
        for (int i = 0; i < 3; i++) {
            int ly = y + sz - 10 + i * 5;
            dsp.drawLine(x + 5, ly, x + sz - 5, ly, COL_BLACK);
        }
        break;

    case 5: // light rain
    case 6: // rain
        drawCloud(dsp, cloudX, cloudY - 8, cloudW, cloudH, COL_BLACK);
        drawRainDrops(dsp, cloudX, cloudY + cloudH - 4, cloudW, type == 5 ? 2 : 3);
        break;

    case 7: // heavy rain
        drawCloud(dsp, cloudX, cloudY - 10, cloudW, cloudH, COL_BLACK);
        drawRainDrops(dsp, cloudX, cloudY + cloudH - 6, cloudW, 4);
        break;

    case 8: // thunder
        drawCloud(dsp, cloudX, cloudY - 10, cloudW, cloudH, COL_BLACK);
        drawRainDrops(dsp, cloudX, cloudY + cloudH - 6, cloudW, 2);
        drawLightning(dsp, x + sz / 2, cloudY + cloudH - 3, sz / 4);
        break;

    case 9: // sleet
        drawCloud(dsp, cloudX, cloudY - 8, cloudW, cloudH, COL_BLACK);
        drawRainDrops(dsp, cloudX, cloudY + cloudH - 4, cloudW / 2, 2);
        drawSnowflakes(dsp, cloudX + cloudW / 2, cloudY + cloudH - 4, cloudW / 2, 2);
        break;

    case 10: // snow
        drawCloud(dsp, cloudX, cloudY - 8, cloudW, cloudH, COL_BLACK);
        drawSnowflakes(dsp, cloudX, cloudY + cloudH - 4, cloudW, 4);
        break;

    case 11: // light snow
        drawCloud(dsp, cloudX, cloudY - 8, cloudW, cloudH, COL_BLACK);
        drawSnowflakes(dsp, cloudX, cloudY + cloudH - 4, cloudW, 2);
        break;

    default:
        drawCloud(dsp, cloudX, cloudY, cloudW, cloudH, COL_BLACK);
        break;
    }
}

void iconDrawWeatherSmall(Display_t &dsp, int16_t x, int16_t y,
                          const char *symbolCode) {
    iconDrawWeather(dsp, x, y, 36, symbolCode);
}

// ---- Info icons ----

void iconDrawSunrise(Display_t &dsp, int16_t x, int16_t y, int16_t sz) {
    int cx = x + sz / 2;
    int horizon = y + sz - 4;
    int r = sz / 3;
    // Half sun above horizon
    for (int dy = 0; dy <= r; dy++) {
        int dx = (int)sqrtf(r * r - dy * dy);
        dsp.drawLine(cx - dx, horizon - dy, cx + dx, horizon - dy, COL_ORANGE);
    }
    // Rays above
    for (int a = -60; a <= 60; a += 30) {
        float rad = a * M_PI / 180.0f;
        int x1 = cx + (int)((r + 1) * sinf(rad));
        int y1 = horizon - (int)((r + 1) * cosf(rad));
        int x2 = cx + (int)((r + 4) * sinf(rad));
        int y2 = horizon - (int)((r + 4) * cosf(rad));
        dsp.drawLine(x1, y1, x2, y2, COL_ORANGE);
    }
    // Horizon line
    dsp.drawLine(x, horizon, x + sz, horizon, COL_BLACK);
    // Up arrow
    dsp.drawLine(cx, y, cx, y + sz / 3, COL_BLACK);
    dsp.drawLine(cx - 3, y + 4, cx, y, COL_BLACK);
    dsp.drawLine(cx + 3, y + 4, cx, y, COL_BLACK);
}

void iconDrawSunset(Display_t &dsp, int16_t x, int16_t y, int16_t sz) {
    int cx = x + sz / 2;
    int horizon = y + sz - 4;
    int r = sz / 3;
    // Half sun at horizon
    for (int dy = 0; dy <= r; dy++) {
        int dx = (int)sqrtf(r * r - dy * dy);
        dsp.drawLine(cx - dx, horizon - dy, cx + dx, horizon - dy, COL_ORANGE);
    }
    // Horizon line
    dsp.drawLine(x, horizon, x + sz, horizon, COL_BLACK);
    // Down arrow
    int arrTop = y + 2;
    int arrBot = y + sz / 3 + 2;
    dsp.drawLine(cx, arrTop, cx, arrBot, COL_BLACK);
    dsp.drawLine(cx - 3, arrBot - 4, cx, arrBot, COL_BLACK);
    dsp.drawLine(cx + 3, arrBot - 4, cx, arrBot, COL_BLACK);
}

void iconDrawHumidity(Display_t &dsp, int16_t x, int16_t y, int16_t sz) {
    int cx = x + sz / 2;
    int r = sz / 3;
    int tipY = y + 2;
    int cenY = y + sz - r - 1;
    // Water droplet: triangle top + circle bottom
    dsp.fillCircle(cx, cenY, r, COL_BLUE);
    dsp.fillTriangle(cx, tipY, cx - r, cenY, cx + r, cenY, COL_BLUE);
}

void iconDrawPressure(Display_t &dsp, int16_t x, int16_t y, int16_t sz) {
    int cx = x + sz / 2;
    int cy = y + sz / 2;
    int r = sz / 2 - 1;
    // Gauge circle
    dsp.drawCircle(cx, cy, r, COL_BLACK);
    dsp.drawCircle(cx, cy, r - 1, COL_BLACK);
    // Needle pointing up-right (~45°)
    float a = -45.0f * M_PI / 180.0f;
    int nx = cx + (int)((r - 3) * sinf(a));
    int ny = cy + (int)((r - 3) * cosf(a));
    dsp.drawLine(cx, cy, nx, ny, COL_RED);
    dsp.fillCircle(cx, cy, 2, COL_BLACK);
    // Tick marks
    for (int d = -90; d <= 90; d += 45) {
        float rad = d * M_PI / 180.0f;
        int tx1 = cx + (int)((r - 2) * sinf(rad));
        int ty1 = cy - (int)((r - 2) * cosf(rad));
        int tx2 = cx + (int)(r * sinf(rad));
        int ty2 = cy - (int)(r * cosf(rad));
        dsp.drawLine(tx1, ty1, tx2, ty2, COL_BLACK);
    }
}

void iconDrawRainProb(Display_t &dsp, int16_t x, int16_t y, int16_t sz) {
    // Small cloud with single raindrop
    int cw = (int)(sz * 0.75f);
    int ch = sz / 3;
    drawCloud(dsp, x + 2, y, cw, ch, COL_BLACK);
    // Single large drop
    int cx = x + sz / 2;
    int dropR = sz / 6;
    int dropCy = y + ch + dropR + 4;
    dsp.fillCircle(cx, dropCy, dropR, COL_BLUE);
    dsp.fillTriangle(cx, y + ch + 1, cx - dropR, dropCy, cx + dropR, dropCy, COL_BLUE);
}

void iconDrawWindArrow(Display_t &dsp, int16_t cx, int16_t cy, int16_t sz,
                       float angleDeg) {
    // Draw an arrow pointing in the direction wind comes FROM
    float rad = angleDeg * M_PI / 180.0f;
    float ca = cosf(rad), sa = sinf(rad);
    int len = sz / 2 - 1;
    int aw  = sz / 5;  // arrowhead half-width

    // Tip (in direction of wind source)
    int tx = cx + (int)(sa * len);
    int ty = cy - (int)(ca * len);
    // Tail
    int bx = cx - (int)(sa * len);
    int by = cy + (int)(ca * len);
    // Shaft
    dsp.drawLine(bx, by, tx, ty, COL_BLACK);
    // Arrowhead
    int ax1 = tx - (int)(sa * aw * 1.5f) - (int)(ca * aw);
    int ay1 = ty + (int)(ca * aw * 1.5f) - (int)(sa * aw);
    int ax2 = tx - (int)(sa * aw * 1.5f) + (int)(ca * aw);
    int ay2 = ty + (int)(ca * aw * 1.5f) + (int)(sa * aw);
    dsp.fillTriangle(tx, ty, ax1, ay1, ax2, ay2, COL_BLACK);

    // Circle outline
    dsp.drawCircle(cx, cy, sz / 2, COL_BLACK);
}

void iconDrawAQI(Display_t &dsp, int16_t cx, int16_t cy, int16_t r, int aqi) {
    uint16_t bg;
    if (aqi <= 2) bg = COL_GREEN;
    else if (aqi <= 4) bg = COL_ORANGE;
    else bg = COL_RED;

    dsp.fillCircle(cx, cy, r, bg);

    // Inner symbol in white
    if (aqi <= 2) {
        // Check mark  ✓
        dsp.drawLine(cx - r/3, cy,     cx - 1,   cy + r/3, COL_WHITE);
        dsp.drawLine(cx - 1,   cy + r/3, cx + r/3, cy - r/3, COL_WHITE);
        dsp.drawLine(cx - r/3 + 1, cy + 1, cx, cy + r/3 + 1, COL_WHITE);
        dsp.drawLine(cx, cy + r/3 + 1, cx + r/3 + 1, cy - r/3 + 1, COL_WHITE);
    } else if (aqi <= 4) {
        // Exclamation !
        dsp.fillRect(cx - 1, cy - r/2, 3, r - 2, COL_WHITE);
        dsp.fillCircle(cx, cy + r/2 - 1, 1, COL_WHITE);
    } else {
        // Close X
        dsp.drawLine(cx - r/3, cy - r/3, cx + r/3, cy + r/3, COL_WHITE);
        dsp.drawLine(cx + r/3, cy - r/3, cx - r/3, cy + r/3, COL_WHITE);
        dsp.drawLine(cx - r/3 + 1, cy - r/3, cx + r/3 + 1, cy + r/3, COL_WHITE);
        dsp.drawLine(cx + r/3 - 1, cy - r/3, cx - r/3 - 1, cy + r/3, COL_WHITE);
    }
}

// ---- UI icons ----

void iconDrawBattery(Display_t &dsp, int16_t x, int16_t y,
                     int16_t w, int16_t h, int pct) {
    // Body outline
    dsp.drawRect(x, y, w - 3, h, COL_BLACK);
    // Terminal nub
    dsp.fillRect(x + w - 3, y + h / 4, 3, h / 2, COL_BLACK);

    // Fill colour based on percentage
    uint16_t fillCol = COL_GREEN;
    if (pct < 20) fillCol = COL_RED;
    else if (pct < 50) fillCol = COL_ORANGE;

    // Fill bar (2px inset)
    int fillW = (int)(((w - 7) * pct) / 100.0f);
    if (fillW > 0)
        dsp.fillRect(x + 2, y + 2, fillW, h - 4, fillCol);
}

void iconDrawRefresh(Display_t &dsp, int16_t cx, int16_t cy, int16_t r) {
    // Circular arrow (refresh / mdiRestore style)
    // Draw partial circle using short line segments
    for (int a = 30; a < 330; a += 5) {
        float r1 = a * M_PI / 180.0f;
        float r2 = (a + 5) * M_PI / 180.0f;
        dsp.drawLine(
            cx + (int)(r * cosf(r1)), cy - (int)(r * sinf(r1)),
            cx + (int)(r * cosf(r2)), cy - (int)(r * sinf(r2)),
            COL_BLACK);
    }
    // Arrowhead at ~30° position
    float aRad = 30.0f * M_PI / 180.0f;
    int ax = cx + (int)(r * cosf(aRad));
    int ay = cy - (int)(r * sinf(aRad));
    dsp.fillTriangle(ax, ay, ax + 5, ay - 3, ax + 3, ay + 4, COL_BLACK);
}
