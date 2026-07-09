#pragma once

#include <Arduino.h>

#include "pins.h"

// RGB-Status-LED (GPIO17/4/16). Blinkt bei anhaltendem Ping-Fehler oder
// ueberschrittenem Warnschwellwert (lastenheft.txt Abschnitt 9 + Nutzer-
// Erweiterung um Warnschwellwerte, siehe docs/entscheidungen.md) - rot bei
// Ausfall/Ueberschreitung, blau bei Unterschreitung. Polaritaet am echten
// Board verifiziert: gemeinsame Anode (LOW=an), nicht wie urspruenglich
// angenommen gemeinsame Kathode - siehe docs/entscheidungen.md.
class LedManager {
public:
	void begin();
	void update(bool alertActive, bool blue = false);

private:
	void setColor(bool r, bool g, bool b);
};
