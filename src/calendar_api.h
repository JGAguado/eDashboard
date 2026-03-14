#pragma once
// ============================================================
// Calendar API — Google Calendar iCal (.ics) feed
// ============================================================

#include <Arduino.h>
#include "config.h"

/// A single calendar event.
struct CalendarEvent {
    char   title[64];
    char   location[48];
    int    startHour, startMin;
    int    endHour,   endMin;
    int    day, month, year;
    uint8_t colorIndex;   // 0-4 for display dot colour
};

/// Calendar data for the current month.
struct CalendarData {
    CalendarEvent events[MAX_CALENDAR_EVENTS];
    int           eventCount;
    int           nextEventIdx;  // index of the next upcoming event (-1 if none)
    bool          ok;            // true if fetch succeeded
};

/// Fetch events for the current month from a Google Calendar iCal URL.
/// The URL should be the secret .ics address from Google Calendar settings.
CalendarData calendarFetch(const char *icalUrl,
                           int year, int month, int day,
                           int hour, int minute);
