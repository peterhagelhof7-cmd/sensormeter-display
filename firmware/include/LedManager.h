#pragma once

#include <Arduino.h>

#include "pins.h"

// RGB-Status-LED (GPIO17/4/16). Aktuell einzige Funktion: rotes Blinken bei
// anhaltendem Ping-Fehler (lastenheft.txt Abschnitt 9). Polaritaet
// (gemeinsame Anode/Kathode) ist im Datenblatt nicht spezifiziert - HIGH=an
// angenommen (gemeinsame Kathode), am Board zu verifizieren, siehe
// docs/entscheidungen.md.
class LedManager {
public:
	void begin();
	void update(bool alertActive);

private:
	void setColor(bool r, bool g, bool b);
};
