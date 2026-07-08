#pragma once

#include "DisplayManager.h"
#include "PingManager.h"

// Zwei Ansichten fuer die Ping-Datenquellen (lastenheft.txt Abschnitt 7.4):
// drawAverage() = durchschnittliche Latenz zu google.com (grosse Zahl),
// drawTargetList() = Liste der zusaetzlich konfigurierten Ziele, je Zeile
// IP + Status (gruen=OK, rot=Fehler).
class PingView {
public:
	void drawAverage(DisplayManager &display, const PingManager &ping, int16_t contentTop,
	                  int16_t contentBottom, uint16_t bgColor);
	void drawTargetList(DisplayManager &display, const PingManager &ping, int16_t contentTop,
	                     int16_t contentBottom, uint16_t bgColor);
};
