// ============================================================
// Calendar API — Google Calendar iCal (.ics) feed
// ============================================================
//
// Fetches a public/secret iCal URL from Google Calendar and
// parses VEVENT entries for the current month.  No OAuth needed —
// just the secret .ics address from Google Calendar settings.
// ============================================================

#include "calendar_api.h"
#include "time_utils.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>

static const char *TAG = "Calendar";

// ---- iCal line helpers ----

/// Unfold iCal continuation lines (lines starting with space/tab are
/// continuations of the previous line).  Modifies `raw` in-place and
/// returns a pointer to it.
static void icalUnfold(String &raw) {
    raw.replace("\r\n ", "");
    raw.replace("\r\n\t", "");
    raw.replace("\n ", "");
    raw.replace("\n\t", "");
}

/// Extract the value from a "KEY:value" or "KEY;params:value" iCal line.
static String icalValue(const String &line) {
    int colon = line.indexOf(':');
    if (colon < 0) return "";
    return line.substring(colon + 1);
}

/// Parse an iCal DTSTART/DTEND value.  Handles:
///   - 20260228T150000Z        (UTC)
///   - 20260228T150000         (local)
///   - TZID=Europe/Berlin:20260228T150000
///   - 20260228                (all-day)
/// Returns true and fills y/m/d/h/min.  For all-day events h=0, min=0.
static bool icalParseDateTime(const String &raw,
                              int &y, int &m, int &d, int &h, int &min) {
    // Find the actual datetime portion (after last colon, if any)
    String dt = raw;
    int colon = dt.lastIndexOf(':');
    if (colon >= 0) dt = dt.substring(colon + 1);
    dt.trim();

    if (dt.length() < 8) return false;

    // Parse date
    y = dt.substring(0, 4).toInt();
    m = dt.substring(4, 6).toInt();
    d = dt.substring(6, 8).toInt();

    // Parse time if present (after 'T')
    if (dt.length() >= 15 && dt[8] == 'T') {
        h   = dt.substring(9, 11).toInt();
        min = dt.substring(11, 13).toInt();
    } else {
        h = 0;
        min = 0;
    }

    return true;
}

// ---- Fetch & parse ----

CalendarData calendarFetch(const char *icalUrl,
                           int year, int month, int day,
                           int hour, int minute) {
    CalendarData cd = {};
    cd.ok = false;
    cd.nextEventIdx = -1;

    // Validate URL
    if (icalUrl == nullptr || strlen(icalUrl) < 10 ||
        strcmp(icalUrl, "YOUR_GOOGLE_ICAL_URL") == 0) {
        Serial.printf("[%s] iCal URL not configured — skipping calendar\n", TAG);
        return cd;
    }

    Serial.printf("[%s] Fetching iCal feed...\n", TAG);
    Serial.printf("[%s] Heap — free: %u  max-block: %u  PSRAM free: %u\n",
                  TAG, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram());
    Serial.printf("[%s] DMA-capable — free: %u  max-block: %u\n",
                  TAG,
                  heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    Serial.printf("[%s] URL length: %d, starts with: %.40s...\n",
                  TAG, strlen(icalUrl), icalUrl);

    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification (IoT trade-off)
    Serial.printf("[%s] SSL client created, heap now: %u  max-block: %u\n",
                  TAG, ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    HTTPClient http;
    http.begin(client, icalUrl);
    http.addHeader("User-Agent", "eDashboard/1.0");
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    Serial.printf("[%s] Sending GET...\n", TAG);
    int code = http.GET();
    Serial.printf("[%s] GET returned code: %d, heap now: %u  max-block: %u\n",
                  TAG, code, ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    if (code != HTTP_CODE_OK) {
        Serial.printf("[%s] iCal fetch FAILED — HTTP %d\n", TAG, code);
        if (code < 0) {
            Serial.printf("[%s] (negative = connection/SSL error, not HTTP status)\n", TAG);
        }
        http.end();
        return cd;
    }

    String ical = http.getString();
    http.end();

    Serial.printf("[%s] Received %d bytes of iCal data\n", TAG, ical.length());

    // Unfold continuation lines
    icalUnfold(ical);

    // Target month range for filtering
    int daysInMonth = timeDaysInMonth(year, month);
    int nowTotal = day * 1440 + hour * 60 + minute;

    // Parse VEVENT blocks
    bool inEvent = false;
    String summary, location, dtstart, dtend;
    uint8_t colorIdx = 0;

    int lineStart = 0;
    while (lineStart < (int)ical.length() && cd.eventCount < MAX_CALENDAR_EVENTS) {
        int lineEnd = ical.indexOf('\n', lineStart);
        if (lineEnd < 0) lineEnd = ical.length();

        String line = ical.substring(lineStart, lineEnd);
        line.trim();
        lineStart = lineEnd + 1;

        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            summary = "";
            location = "";
            dtstart = "";
            dtend = "";
            continue;
        }

        if (line == "END:VEVENT") {
            inEvent = false;

            // Parse start/end times
            int sy, sm, sd, sh, smin;
            int ey, em, ed, eh, emin;
            if (!icalParseDateTime(dtstart, sy, sm, sd, sh, smin)) continue;
            if (!icalParseDateTime(dtend, ey, em, ed, eh, emin)) {
                ey = sy; em = sm; ed = sd; eh = sh; emin = smin;
            }

            // Filter: keep events that overlap with current month
            bool endBeforeMonth = (ey < year || (ey == year && em < month));
            bool afterMonth  = (sy > year || (sy == year && sm > month) ||
                                (sy == year && sm == month && sd > daysInMonth));

            if (endBeforeMonth || afterMonth) continue;

            // This event overlaps with our month — store it
            CalendarEvent &e = cd.events[cd.eventCount];
            strlcpy(e.title, summary.c_str(), sizeof(e.title));
            strlcpy(e.location, location.c_str(), sizeof(e.location));
            e.year = sy;
            e.month = sm;
            e.day = sd;
            e.startHour = sh;
            e.startMin = smin;
            e.endHour = eh;
            e.endMin = emin;
            e.colorIndex = colorIdx % 5;
            colorIdx++;

            // Track next upcoming event
            if (sy == year && sm == month) {
                int evTotal = sd * 1440 + sh * 60 + smin;
                if (evTotal >= nowTotal && cd.nextEventIdx < 0) {
                    cd.nextEventIdx = cd.eventCount;
                }
            }

            cd.eventCount++;
            continue;
        }

        if (!inEvent) continue;

        // Parse event properties
        if (line.startsWith("SUMMARY")) {
            summary = icalValue(line);
        } else if (line.startsWith("LOCATION")) {
            location = icalValue(line);
        } else if (line.startsWith("DTSTART")) {
            dtstart = line;  // keep full line for TZID parsing
        } else if (line.startsWith("DTEND")) {
            dtend = line;
        }
    }

    cd.ok = (cd.eventCount >= 0);
    Serial.printf("[%s] Parsed %d events for %04d-%02d, next event idx=%d\n",
                  TAG, cd.eventCount, year, month, cd.nextEventIdx);
    return cd;
}
