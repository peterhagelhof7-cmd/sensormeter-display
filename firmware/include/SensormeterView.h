#pragma once

#include "DisplayManager.h"
#include "SensormeterManager.h"

// Ansicht fuer die Sensormeter-Datenquelle (SNMP-Abfrage anderer
// Netzwerkgeraete, siehe lastenheft.txt Abschnitt 7.3). Kein Verlauf/Graph
// gefordert, nur der aktuelle Wert. Da mehrere Ziele (und bei PRO-Geraeten
// je zwei Sensoren) konfiguriert sein koennen, rotiert die Ansicht intern
// mit eigenem Timer durch alle aufgeloesten Sensor-Slides - unabhaengig
// davon, wie oft draw() vom Hauptloop aufgerufen wird (analog zum
// WLAN-Blink-Symbol in StatusBar, das ebenfalls per millis() selbst
// entscheidet, wann es umschaltet).
class SensormeterView {
public:
	void draw(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
	          int16_t contentBottom, uint16_t bgColor);

private:
	static constexpr uint32_t kSubSlideIntervalMs = 4000;

	size_t currentSlide_ = 0;
	uint32_t lastRotateMs_ = 0;
};
