#include "TimeSync.h"

#include <time.h>

namespace {
const char *kWeekdaysDe[7] = {"Sonntag", "Montag", "Dienstag", "Mittwoch",
                              "Donnerstag", "Freitag", "Samstag"};
}

void TimeSync::begin() {
	// M3.5.0 = letzter Sonntag im Maerz (Sommerzeit-Beginn),
	// M10.5.0/3 = letzter Sonntag im Oktober, 3 Uhr (Sommerzeit-Ende).
	configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "de.pool.ntp.org");
}

bool TimeSync::isValid() {
	struct tm t;
	if (!getLocalTime(&t, 10)) {
		return false;
	}
	return (t.tm_year + 1900) > 2020;
}

String TimeSync::formatTime() {
	struct tm t;
	if (!getLocalTime(&t, 10) || (t.tm_year + 1900) <= 2020) {
		return "";
	}
	char buf[6];
	snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
	return String(buf);
}

String TimeSync::formatDate() {
	struct tm t;
	if (!getLocalTime(&t, 10) || (t.tm_year + 1900) <= 2020) {
		return "";
	}
	char buf[32];
	snprintf(buf, sizeof(buf), "%s, %02d.%02d.%04d", kWeekdaysDe[t.tm_wday], t.tm_mday,
	         t.tm_mon + 1, t.tm_year + 1900);
	return String(buf);
}
