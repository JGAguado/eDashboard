// ============================================================
// Weather API — met.no locationforecast + OWM air quality
// ============================================================

#include "weather_api.h"
#include "time_utils.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const char *TAG = "Weather";

// ---- met.no locationforecast ----

static bool fetchMetNo(WeatherData &wd, float lat, float lon,
                        int year, int month, int day) {
    Serial.printf("[%s] Free heap before met.no: %u\n", TAG, ESP.getFreeHeap());

    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification (IoT trade-off)

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url),
             "%s?lat=%.4f&lon=%.4f",
             MET_NO_BASE_URL, lat, lon);

    Serial.printf("[%s] GET %s\n", TAG, url);
    http.begin(client, url);
    http.addHeader("User-Agent", MET_NO_USER_AGENT);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[%s] met.no returned %d\n", TAG, code);
        http.end();
        return false;
    }

    // Read the full response into a String — more reliable than
    // streaming from the socket (avoids IncompleteInput on large
    // payloads).  The /compact endpoint is ~40 KB.
    String payload = http.getString();
    http.end();  // Release HTTP + SSL resources immediately

    Serial.printf("[%s] met.no response: %d bytes, free heap: %u\n",
                  TAG, payload.length(), ESP.getFreeHeap());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    payload = String();  // Free response memory before processing

    if (err) {
        Serial.printf("[%s] JSON parse error: %s\n", TAG, err.c_str());
        return false;
    }

    JsonArray timeseries = doc["properties"]["timeseries"];
    if (timeseries.isNull() || timeseries.size() == 0) {
        Serial.printf("[%s] No timeseries data\n", TAG);
        return false;
    }

    // --- Current conditions (first entry) ---
    JsonObject first    = timeseries[0];
    JsonObject instant  = first["data"]["instant"]["details"];
    wd.currentTemp      = instant["air_temperature"]            | 0.0f;
    wd.humidity         = instant["relative_humidity"]           | 0.0f;
    wd.pressure         = instant["air_pressure_at_sea_level"]  | 0.0f;
    wd.windSpeed        = instant["wind_speed"]                 | 0.0f;
    wd.windDirection    = instant["wind_from_direction"]         | 0.0f;

    // Symbol code & precipitation from next_1_hours (or next_6_hours)
    JsonObject n1h = first["data"]["next_1_hours"];
    JsonObject n6h = first["data"]["next_6_hours"];
    if (!n1h.isNull()) {
        strlcpy(wd.symbolCode, n1h["summary"]["symbol_code"] | "cloudy", sizeof(wd.symbolCode));
        wd.precipitation = n1h["details"]["precipitation_amount"] | 0.0f;
    } else if (!n6h.isNull()) {
        strlcpy(wd.symbolCode, n6h["summary"]["symbol_code"] | "cloudy", sizeof(wd.symbolCode));
        wd.precipitation = n6h["details"]["precipitation_amount"] | 0.0f;
    }

    // --- Today's min/max ---
    // Scan entries for today and collect temperature extremes.
    char todayPrefix[12];
    snprintf(todayPrefix, sizeof(todayPrefix), "%04d-%02d-%02d", year, month, day);

    float tMin =  999.0f;
    float tMax = -999.0f;

    for (JsonObject entry : timeseries) {
        const char *timeStr = entry["time"] | "";
        if (strncmp(timeStr, todayPrefix, 10) != 0) continue;

        float t = entry["data"]["instant"]["details"]["air_temperature"] | 0.0f;
        if (t < tMin) tMin = t;
        if (t > tMax) tMax = t;
    }
    wd.minTemp = (tMin < 900.0f) ? tMin : wd.currentTemp;
    wd.maxTemp = (tMax > -900.0f) ? tMax : wd.currentTemp;

    // --- 3-day forecast ---
    // For each of the next 3 days, find the entry closest to 12:00 local,
    // and scan for day min/max.
    for (int d = 0; d < 3; d++) {
        // Compute the target date
        // Simple approach: add days to current date
        int fy = year, fm = month, fd = day + d + 1;
        while (fd > timeDaysInMonth(fy, fm)) {
            fd -= timeDaysInMonth(fy, fm);
            fm++;
            if (fm > 12) { fm = 1; fy++; }
        }

        char prefix[12];
        snprintf(prefix, sizeof(prefix), "%04d-%02d-%02d", fy, fm, fd);

        float dMin =  999.0f;
        float dMax = -999.0f;
        const char *bestSymbol = "cloudy";
        int bestHourDist = 999;

        for (JsonObject entry : timeseries) {
            const char *timeStr = entry["time"] | "";
            if (strncmp(timeStr, prefix, 10) != 0) continue;

            float t = entry["data"]["instant"]["details"]["air_temperature"] | 0.0f;
            if (t < dMin) dMin = t;
            if (t > dMax) dMax = t;

            // Parse hour from ISO 8601
            int hour = 12;
            if (strlen(timeStr) >= 13) {
                hour = (timeStr[11] - '0') * 10 + (timeStr[12] - '0');
            }
            int dist = abs(hour - 12);

            // Prefer entry with next_6_hours or next_1_hours symbol near noon
            if (dist < bestHourDist) {
                JsonObject n = entry["data"]["next_6_hours"];
                if (n.isNull()) n = entry["data"]["next_1_hours"];
                if (!n.isNull()) {
                    bestSymbol = n["summary"]["symbol_code"] | "cloudy";
                    bestHourDist = dist;
                }
            }
        }

        int wday = timeDayOfWeek(fy, fm, fd);
        strlcpy(wd.forecast[d].dayAbbr, timeDayAbbr(wday).c_str(), sizeof(wd.forecast[d].dayAbbr));
        strlcpy(wd.forecast[d].symbolCode, bestSymbol, sizeof(wd.forecast[d].symbolCode));
        wd.forecast[d].maxTemp = (dMax > -900.0f) ? dMax : 0.0f;
        wd.forecast[d].minTemp = (dMin <  900.0f) ? dMin : 0.0f;
    }

    Serial.printf("[%s] met.no OK — %.1f°C (%s), min %.1f / max %.1f\n",
                  TAG, wd.currentTemp, wd.symbolCode, wd.minTemp, wd.maxTemp);
    return true;
}

// ---- OpenWeatherMap Air Quality ----

static bool fetchAirQuality(WeatherData &wd, float lat, float lon, const char *apiKey) {
    if (strlen(apiKey) < 5 || strcmp(apiKey, "YOUR_OWM_API_KEY") == 0) {
        Serial.printf("[%s] OWM API key not configured — skipping AQI\n", TAG);
        wd.aqi = 0;
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url),
             "%s?lat=%.4f&lon=%.4f&appid=%s",
             OWM_AQI_BASE_URL, lat, lon, apiKey);

    Serial.printf("[%s] GET %s\n", TAG, url);
    http.begin(client, url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[%s] OWM AQI returned %d\n", TAG, code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[%s] AQI JSON parse error: %s\n", TAG, err.c_str());
        return false;
    }

    // The AQI is in list[0].main.aqi  (1-5 scale)
    wd.aqi = doc["list"][0]["main"]["aqi"] | 0;
    Serial.printf("[%s] Air quality index: %d\n", TAG, wd.aqi);
    return true;
}

// ---- Public API ----

WeatherData weatherFetchAll(float lat, float lon,
                            const char *owmApiKey,
                            int year, int month, int day,
                            float tzOffsetHours) {
    WeatherData wd = {};

    // 1. Met.no forecast
    wd.weatherOk = fetchMetNo(wd, lat, lon, year, month, day);

    // 2. OpenWeatherMap air quality
    wd.aqiOk = fetchAirQuality(wd, lat, lon, owmApiKey);

    // 3. Sunrise / sunset (local calculation)
    if (!timeSunCalc(lat, lon, year, month, day, tzOffsetHours,
                     wd.sunriseH, wd.sunriseM, wd.sunsetH, wd.sunsetM)) {
        // Polar conditions — set defaults
        wd.sunriseH = 0; wd.sunriseM = 0;
        wd.sunsetH  = 0; wd.sunsetM  = 0;
    }

    return wd;
}
