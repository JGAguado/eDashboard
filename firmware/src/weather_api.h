#pragma once
// ============================================================
// Weather API — met.no forecast + OpenWeatherMap air quality
// ============================================================

#include <Arduino.h>

/// Single day's forecast summary.
struct ForecastDay {
    char dayAbbr[4];     // "Mon", "Tue", ...
    char symbolCode[32]; // met.no symbol_code
    float maxTemp;
    float minTemp;
};

/// Weather data collected from APIs.
struct WeatherData {
    // Current conditions
    float  currentTemp;
    float  minTemp;        // Today's min
    float  maxTemp;        // Today's max
    float  humidity;       // %
    float  pressure;       // hPa
    float  windSpeed;      // m/s
    float  windDirection;  // degrees (0=N, 90=E …)
    float  precipitation; // mm (next 1 h precipitation_amount)
    char   symbolCode[32]; // met.no symbol_code for current weather

    // Sun
    int    sunriseH, sunriseM;
    int    sunsetH,  sunsetM;

    // Air quality (OpenWeatherMap)
    int    aqi;            // 1 (Good) – 5 (Very Poor)

    // 3-day forecast
    ForecastDay forecast[3];

    // Status flags
    bool   weatherOk;
    bool   aqiOk;
};

/// Fetch all weather data.  Requires WiFi to be connected.
WeatherData weatherFetchAll(float lat, float lon,
                            const char *owmApiKey,
                            int year, int month, int day,
                            float tzOffsetHours);
