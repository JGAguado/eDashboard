#pragma once

// ============================================================
// Hardware pin definitions — Seeed Studio reTerminal E1002
// ============================================================

// --- E-Paper display (SPI) ---
#define EPD_SCK   7
#define EPD_MISO  8   // shared with SD card
#define EPD_MOSI  9
#define EPD_CS    10
#define EPD_DC    11
#define EPD_RST   12
#define EPD_BUSY  13

// --- SD card ---
#define SD_CS     14
#define SD_EN     16
#define SD_DET    15

// --- Battery ---
#define BAT_ADC   1    // ADC pin (x2 voltage divider)
#define BAT_EN    21   // Enable battery monitoring circuit

// --- Buttons ---
#define BTN_WAKE  3    // Green refresh / wake button
#define BTN_RIGHT 4    // Right navigation
#define BTN_LEFT  5    // Left navigation

// --- Indicators ---
#define LED_GREEN 6    // User LED (inverted logic)
#define BUZZER    45

// --- I2C (for expansion) ---
#define I2C_SDA   19
#define I2C_SCL   20

// ============================================================
// Display dimensions
// ============================================================
#define DISPLAY_WIDTH   800
#define DISPLAY_HEIGHT  480

// ============================================================
// Layout constants (pixels)
// ============================================================

// Header
#define HEADER_H          55
#define HEADER_SEP_Y      54

// Two-column split
#define COL_SPLIT_X       400
#define CONTENT_Y         (HEADER_H + 1)

// Weather column (left)
#define WX_X              0
#define WX_W              COL_SPLIT_X

// Current weather section
#define WX_CUR_Y          (CONTENT_Y + 3)
#define WX_CUR_H          135
#define WX_CUR_ICON_X     15
#define WX_CUR_ICON_Y     (WX_CUR_Y + 10)
#define WX_CUR_ICON_SZ    80

// Info grid (3 columns × 2 rows)
#define WX_INFO_Y         (WX_CUR_Y + WX_CUR_H + 5)
#define WX_INFO_H         180
#define WX_INFO_COLS      3
#define WX_INFO_ROWS      2
#define WX_INFO_CW        (WX_W / WX_INFO_COLS)
#define WX_INFO_RH        (WX_INFO_H / WX_INFO_ROWS)

// 3-day forecast
#define WX_FCST_Y         (WX_INFO_Y + WX_INFO_H + 5)
#define WX_FCST_H         (DISPLAY_HEIGHT - WX_FCST_Y)
#define WX_FCST_COLS      3
#define WX_FCST_CW        (WX_W / WX_FCST_COLS)

// Calendar column (right)
#define CAL_X             (COL_SPLIT_X)
#define CAL_W             (DISPLAY_WIDTH - COL_SPLIT_X)

// Calendar grid
#define CAL_GRID_Y        (CONTENT_Y + 2)
#define CAL_GRID_H        305
#define CAL_COLS          7
#define CAL_HEADER_H      22
#define CAL_CW            (CAL_W / CAL_COLS)

// Next event
#define CAL_EVENT_Y       (CAL_GRID_Y + CAL_GRID_H + 5)

// ============================================================
// Network / API
// ============================================================
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RETRY_COUNT          3
#define HTTP_TIMEOUT_MS           15000

#define MET_NO_USER_AGENT         "eDashboard/1.0 github.com/your-repo"
#define MET_NO_BASE_URL           "https://api.met.no/weatherapi/locationforecast/2.0/compact"
#define OWM_AQI_BASE_URL          "https://api.openweathermap.org/data/2.5/air_pollution"

// ============================================================
// Battery thresholds (LiPo 3.7 V nominal)
// ============================================================
#define BAT_VOLTAGE_MAX   4.20f
#define BAT_VOLTAGE_MIN   3.00f

// ============================================================
// Misc
// ============================================================
#define MAX_CALENDAR_EVENTS  50
#define MAX_FORECAST_DAYS    3
#define SERIAL_BAUD          115200
