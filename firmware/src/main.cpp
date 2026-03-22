// ============================================================
// eDashboard — main firmware for Seeed Studio reTerminal E1002
//
// Workflow: Wake → WiFi → NTP → Weather → Calendar → Battery
//           → Render display → Deep sleep
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "secrets_gen.h"
#include "wifi_manager.h"
#include "time_utils.h"
#include "weather_api.h"
#include "calendar_api.h"
#include "battery.h"
#include "display_renderer.h"
#include "backend_client.h"
#include "icons.h"

// ---- Display instance (global, PSRAM-backed buffer) ----
Display_t display(GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ============================================================
// setup()  — runs once per wake cycle
// ============================================================

void setup() {
    // 1. Serial logging
    Serial.begin(SERIAL_BAUD);
    delay(200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  eDashboard — reTerminal E1002");
    Serial.println("========================================");

    // Print wake-up reason
    esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
    switch (wakeReason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[Main] Wake: scheduled timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[Main] Wake: button press (EXT0)");
            break;
        default:
            Serial.printf("[Main] Wake: power-on / reset (%d)\n", wakeReason);
            break;
    }

    // LED on during processing
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);  // inverted logic — LOW = on

    // Button pin setup
    pinMode(BTN_WAKE, INPUT_PULLUP);

    // NOTE: Display init is deferred until after network calls.
    // SPI.begin() + GxEPD2 init allocate DMA buffers in internal
    // RAM that fragment the heap and prevent SSL from working.

    // 2. Initialise battery reading
    batteryInit();

    // 3. Connect to WiFi
    bool wifiOk = wifiConnect();
    if (!wifiOk) {
        Serial.println("[Main] WiFi failed — rendering with stale data");
    }

    // 4. Sync NTP time
    if (wifiOk) {
        timeSyncNTP();
    }

    struct tm now = timeGetLocal();
    int year  = now.tm_year + 1900;
    int month = now.tm_mon + 1;
    int day   = now.tm_mday;

    Serial.printf("[Main] Date: %04d-%02d-%02d  Time: %02d:%02d:%02d\n",
                  year, month, day, now.tm_hour, now.tm_min, now.tm_sec);

    // Estimate timezone offset in hours for sunrise/sunset calc
    // (derived from the POSIX TZ string — take the first numeric value)
    float tzOffset = 0;
    {
        const char *tz = SECRET_TIMEZONE;
        // Find first digit or sign
        while (*tz && !isdigit(*tz) && *tz != '-' && *tz != '+') tz++;
        if (*tz) {
            tzOffset = -atof(tz);  // POSIX TZ is inverted: "CET-1" means UTC+1
        }
    }
    Serial.printf("[Main] Timezone offset estimate: %.1f h\n", tzOffset);

    Serial.printf("[Main] Heap before API calls — free: %u  max-block: %u  PSRAM: %u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram());

    // 5. Fetch calendar data FIRST (needs clean heap for SSL/TLS)
    static CalendarData calendar;
    memset(&calendar, 0, sizeof(calendar));
    if (wifiOk) {
        calendar = calendarFetch(
            SECRET_GOOGLE_ICAL_URL,
            year, month, day, now.tm_hour, now.tm_min);
    }
    Serial.printf("[Main] Heap after calendar — free: %u  max-block: %u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // 6. Fetch weather data (large JSON response, ok to fragment heap)
    static WeatherData weather;
    memset(&weather, 0, sizeof(weather));
    if (wifiOk) {
        weather = weatherFetchAll(
            SECRET_LATITUDE, SECRET_LONGITUDE,
            SECRET_OWM_API_KEY,
            year, month, day, tzOffset);
    }
    Serial.printf("[Main] Heap after weather — free: %u  max-block: %u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    // 7. Read battery
    BatteryData battery = batteryRead();

    // 8. Now initialise display hardware (deferred to free DMA memory for SSL)
    displayInit(display);

    // 9. Prefer pulling the pre-rendered image from backend.
    bool backendRendered = false;
    if (wifiOk) {
        backendRendered = backendRenderFromBinary(display, SECRET_BACKEND_IMAGE_URL);
    }

    // 10. Fallback to onboard renderer if backend image is unavailable.
    if (!backendRendered) {
        bool useCelsius = (strcmp(SECRET_TEMP_UNIT, "C") == 0);
        static DashboardData dashData;
        memset(&dashData, 0, sizeof(dashData));
        dashData.now           = now;
        dashData.battery       = battery;
        dashData.weather       = weather;
        dashData.calendar      = calendar;
        dashData.useCelsius    = useCelsius;
        strlcpy(dashData.windUnit, SECRET_WIND_UNIT, sizeof(dashData.windUnit));
        dashData.tzOffsetHours = tzOffset;

        displayRender(display, dashData);
    }

    // 11. Disconnect WiFi to save power
    wifiDisconnect();

    // 12. Hibernate the display
    display.hibernate();

    // 13. LED off
    digitalWrite(LED_GREEN, HIGH);  // inverted logic — HIGH = off

    // 14. Calculate sleep time & enter deep sleep
    now = timeGetLocal();  // refresh after rendering
    uint64_t sleepUs = timeCalcSleepUs(SECRET_SLEEP_MINUTES,
                                        SECRET_NIGHT_START,
                                        SECRET_NIGHT_END, now);

    Serial.printf("[Main] Entering deep sleep for %llu seconds...\n",
                  (unsigned long long)(sleepUs / 1000000ULL));
    Serial.flush();

    // Enable wake-up sources
    esp_sleep_enable_timer_wakeup(sleepUs);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_WAKE, 0);  // Wake on button LOW

    esp_deep_sleep_start();
    // --- Device is now asleep; execution resumes at setup() on next wake ---
}

// ============================================================
// loop()  — never reached (deep sleep restarts from setup)
// ============================================================

void loop() {
    // This function is intentionally empty.
    // The device enters deep sleep at the end of setup() and
    // re-executes setup() on every wake cycle.
}
