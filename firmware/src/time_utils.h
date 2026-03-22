#pragma once
// ============================================================
// Time utilities — NTP sync, formatting, sunrise/sunset calc
// ============================================================

#include <Arduino.h>
#include <time.h>

/// Synchronise the ESP32 RTC with an NTP server.
/// Call after WiFi is connected.  Returns true on success.
bool timeSyncNTP();

/// Return the current local time in a struct tm.
struct tm timeGetLocal();

/// Format the date for the header, e.g. "Saturday, 28 February 2026".
String timeFormatDate(const struct tm &t);

/// Format HH:MM from a struct tm.
String timeFormatHM(const struct tm &t);

/// Format HH:MM from hour/minute ints.
String timeFormatHM(int h, int m);

/// Day-of-week abbreviation (Mon, Tue, ...).
String timeDayAbbr(int wday);

/// Full day-of-week name.
String timeDayName(int wday);

/// Full month name.
String timeMonthName(int mon);

/// Number of days in a given month (1-12) / year.
int timeDaysInMonth(int year, int month);

/// Day of week for a given date (0=Sunday, 1=Monday, ..., 6=Saturday).
int timeDayOfWeek(int year, int month, int day);

/// Compute sunrise and sunset times (local hour.fraction) using
/// simplified NOAA solar equations.  Returns false if polar conditions.
bool timeSunCalc(float lat, float lon, int year, int month, int day,
                 float tzOffsetHours,
                 int &sunriseH, int &sunriseM,
                 int &sunsetH,  int &sunsetM);

/// Calculate the number of microseconds to sleep.
/// Uses night-mode (extended sleep between nightStart and nightEnd hours)
/// and a regular sleep interval (in minutes) during the day.
/// During night-mode hours, sleeps until nightEnd.
uint64_t timeCalcSleepUs(int sleepMinutes, int nightStart, int nightEnd,
                         const struct tm &now);
