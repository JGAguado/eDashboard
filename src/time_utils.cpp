// ============================================================
// Time utilities — NTP sync, formatting, sunrise/sunset calc
// ============================================================

#include "time_utils.h"
#include "config.h"
#include "secrets_gen.h"
#include <math.h>

static const char *TAG = "Time";

// ---- NTP synchronisation ----

bool timeSyncNTP() {
    Serial.printf("[%s] Syncing with NTP (%s) TZ=%s\n",
                  TAG, SECRET_NTP_SERVER, SECRET_TIMEZONE);

    configTzTime(SECRET_TIMEZONE, SECRET_NTP_SERVER, "time.google.com");

    // Wait up to 10 s for time to be set
    struct tm t;
    for (int i = 0; i < 40; i++) {
        if (getLocalTime(&t, 250)) {
            Serial.printf("[%s] Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                          TAG, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                          t.tm_hour, t.tm_min, t.tm_sec);
            return true;
        }
    }
    Serial.printf("[%s] NTP sync failed!\n", TAG);
    return false;
}

// ---- Current time ----

struct tm timeGetLocal() {
    struct tm t;
    getLocalTime(&t);
    return t;
}

// ---- Formatting ----

static const char *DAY_NAMES[]  = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char *DAY_ABBR[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *MONTH_NAMES[] = {"January","February","March","April","May","June",
                                    "July","August","September","October","November","December"};

String timeFormatDate(const struct tm &t) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s, %d %s %d",
             DAY_NAMES[t.tm_wday], t.tm_mday,
             MONTH_NAMES[t.tm_mon], t.tm_year + 1900);
    return String(buf);
}

String timeFormatHM(const struct tm &t) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

String timeFormatHM(int h, int m) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return String(buf);
}

String timeDayAbbr(int wday) { return String(DAY_ABBR[wday % 7]); }
String timeDayName(int wday) { return String(DAY_NAMES[wday % 7]); }
String timeMonthName(int mon) { return String(MONTH_NAMES[mon % 12]); }

int timeDaysInMonth(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = days[(month - 1) % 12];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        d = 29;
    return d;
}

int timeDayOfWeek(int year, int month, int day) {
    // Tomohiko Sakamoto's algorithm — returns 0=Sunday
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (year + year/4 - year/100 + year/400 + t[month - 1] + day) % 7;
}

// ---- Sunrise / sunset (simplified NOAA) ----

static double degToRad(double d) { return d * M_PI / 180.0; }
static double radToDeg(double r) { return r * 180.0 / M_PI; }

bool timeSunCalc(float lat, float lon, int year, int month, int day,
                 float tzOffsetHours,
                 int &sunriseH, int &sunriseM,
                 int &sunsetH,  int &sunsetM)
{
    // Day of year
    int N1 = (int)(275.0 * month / 9.0);
    int N2 = (int)((month + 9.0) / 12.0);
    int N3 = (int)(1 + (year - 4 * (int)(year / 4.0) + 2) / 3.0);
    int N  = N1 - (N2 * N3) + day - 30;

    double lngHour = lon / 15.0;

    // Sunrise / sunset calculations — iterate for both
    for (int isSunset = 0; isSunset <= 1; isSunset++) {
        double tApprox = N + ((isSunset ? 18.0 : 6.0) - lngHour) / 24.0;

        // Sun's mean anomaly
        double M = (0.9856 * tApprox) - 3.289;

        // Sun's true longitude
        double L = M + (1.916 * sin(degToRad(M))) + (0.020 * sin(degToRad(2 * M))) + 282.634;
        while (L < 0)   L += 360.0;
        while (L >= 360) L -= 360.0;

        // Right ascension
        double RA = radToDeg(atan(0.91764 * tan(degToRad(L))));
        while (RA < 0)   RA += 360.0;
        while (RA >= 360) RA -= 360.0;

        // Adjust RA to same quadrant as L
        int lQ  = (int)(L  / 90.0) * 90;
        int raQ = (int)(RA / 90.0) * 90;
        RA += (lQ - raQ);
        RA /= 15.0;  // convert to hours

        // Sun's declination
        double sinDec = 0.39782 * sin(degToRad(L));
        double cosDec = cos(asin(sinDec));

        // Sun's local hour angle
        double zenith = 90.833;  // official with refraction correction
        double cosH = (cos(degToRad(zenith)) - (sinDec * sin(degToRad(lat)))) /
                      (cosDec * cos(degToRad(lat)));

        if (cosH > 1.0 || cosH < -1.0) {
            // Sun never rises / never sets at this location on this date
            return false;
        }

        double H;
        if (isSunset)
            H = radToDeg(acos(cosH));
        else
            H = 360.0 - radToDeg(acos(cosH));
        H /= 15.0;

        // Local mean time
        double T = H + RA - (0.06571 * tApprox) - 6.622;

        // UTC time
        double UT = T - lngHour;
        while (UT < 0)  UT += 24.0;
        while (UT >= 24) UT -= 24.0;

        // Apply timezone
        double local = UT + tzOffsetHours;
        while (local < 0)  local += 24.0;
        while (local >= 24) local -= 24.0;

        int hour = (int)local;
        int minute = (int)((local - hour) * 60.0);

        if (isSunset) {
            sunsetH = hour;
            sunsetM = minute;
        } else {
            sunriseH = hour;
            sunriseM = minute;
        }
    }

    Serial.printf("[%s] Sun: rise %02d:%02d  set %02d:%02d\n",
                  TAG, sunriseH, sunriseM, sunsetH, sunsetM);
    return true;
}

// ---- Sleep time calculation ----

uint64_t timeCalcSleepUs(int sleepMinutes, int nightStart, int nightEnd,
                         const struct tm &now) {
    int nowMinutes = now.tm_hour * 60 + now.tm_min;

    // Determine if we are in night-mode hours
    bool inNight = false;
    if (nightStart != nightEnd) {
        if (nightStart < nightEnd) {
            // e.g. nightStart=23:00 nightEnd=07:00 won't hit here
            inNight = (nowMinutes >= nightStart * 60 && nowMinutes < nightEnd * 60);
        } else {
            // Wraps midnight: e.g. 23:00 → 07:00
            inNight = (nowMinutes >= nightStart * 60 || nowMinutes < nightEnd * 60);
        }
    }

    int sleepMin;
    if (inNight) {
        // Sleep until nightEnd
        int target = nightEnd * 60;  // minutes from midnight
        int delta = target - nowMinutes;
        if (delta <= 0) delta += 1440;  // wrap to next day
        sleepMin = delta;
        Serial.printf("[%s] Night mode — sleeping until %02d:00 (%d min)\n",
                      TAG, nightEnd, sleepMin);
    } else {
        // Regular interval
        sleepMin = (sleepMinutes > 0) ? sleepMinutes : 60;
        Serial.printf("[%s] Day mode — sleeping for %d min\n", TAG, sleepMin);
    }

    // Add 30 s margin
    uint64_t sleepSec = (uint64_t)sleepMin * 60ULL + 30ULL;
    return sleepSec * 1000000ULL;
}
