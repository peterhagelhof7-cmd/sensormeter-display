#include "LedManager.h"

void LedManager::begin() {
	pinMode(LED_R_PIN, OUTPUT);
	pinMode(LED_G_PIN, OUTPUT);
	pinMode(LED_B_PIN, OUTPUT);
	setColor(false, false, false);
}

void LedManager::setColor(bool r, bool g, bool b) {
	// Hardware-Befund: Polaritaet war umgekehrt zur urspruenglichen Annahme
	// (gemeinsame Kathode, HIGH=an) - die LED leuchtete dauerhaft im
	// vermeintlichen "aus"-Zustand. Tatsaechlich gemeinsame Anode (LOW=an),
	// daher hier invertiert. Siehe docs/entscheidungen.md.
	digitalWrite(LED_R_PIN, r ? LOW : HIGH);
	digitalWrite(LED_G_PIN, g ? LOW : HIGH);
	digitalWrite(LED_B_PIN, b ? LOW : HIGH);
}

void LedManager::update(bool alertActive) {
	if (!alertActive) {
		setColor(false, false, false);
		return;
	}
	bool on = (millis() / 500) % 2 == 0;
	setColor(on, false, false);
}
