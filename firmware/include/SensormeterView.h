#pragma once

#include "DisplayManager.h"
#include "SensormeterClient.h"

// Ansicht fuer die Sensormeter-Datenquelle (SNMP-Abfrage eines anderen
// Netzwerkgeraets, siehe lastenheft.txt Abschnitt 7.3). Kein Verlauf/Graph
// gefordert, nur der aktuelle Wert.
class SensormeterView {
public:
	void draw(DisplayManager &display, const SensormeterClient &client, int16_t contentTop,
	          int16_t contentBottom, uint16_t bgColor);
};
