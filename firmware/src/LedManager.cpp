#include "LedManager.h"

void LedManager::begin() {
	pinMode(LED_R_PIN, OUTPUT);
	pinMode(LED_G_PIN, OUTPUT);
	pinMode(LED_B_PIN, OUTPUT);
	setColor(false, false, false);
}

void LedManager::setColor(bool r, bool g, bool b) {
	digitalWrite(LED_R_PIN, r ? HIGH : LOW);
	digitalWrite(LED_G_PIN, g ? HIGH : LOW);
	digitalWrite(LED_B_PIN, b ? HIGH : LOW);
}

void LedManager::update(bool alertActive) {
	if (!alertActive) {
		setColor(false, false, false);
		return;
	}
	bool on = (millis() / 500) % 2 == 0;
	setColor(on, false, false);
}
