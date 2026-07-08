#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "DisplayManager.h"
#include "pins.h"

// Touch-Controller (XPT2046-artig) wird per Bit-Bang auf TP_CLK/CS/DIN/OUT
// angesteuert (kein Hardware-SPI, um Kollisionen mit dem TFT-Bus zu
// vermeiden - siehe platformio.ini). TP_IRQ zeigt per Low-Pegel eine aktive
// Beruehrung an. Kalibrierdaten (Rohwert-Extrema je Achse) werden in NVS
// (Preferences, Namespace "touch") gespeichert und beim Boot geladen; ohne
// gespeicherte Daten muss runCalibration() vor der ersten Nutzung laufen.
//
// Protokoll-Timing ist Standard-XPT2046 (Kommandobyte MSB-first, 12-Bit-
// Ergebnis), aber an diesem konkreten Board noch nicht auf echter Hardware
// verifiziert - siehe docs/entscheidungen.md.
class TouchManager {
public:
	void begin();

	bool isCalibrated() const { return calibrated; }
	// Zwei-Punkt-Kalibrierung: Ziel oben-links, dann unten-rechts antippen.
	void runCalibration(DisplayManager &display);

	// true, wenn aktuell beruehrt; screenX/screenY in Bildschirmkoordinaten
	// (0..319 x, 0..239 y), nur gueltig wenn isCalibrated().
	bool read(int16_t &screenX, int16_t &screenY);

private:
	void loadCalibration();
	void saveCalibration();
	int16_t mapAxis(int32_t raw, int32_t rawMin, int32_t rawMax, int16_t outMax) const;

	bool touched() const;
	uint16_t readChannel(uint8_t command) const;
	bool readRaw(int32_t &rawX, int32_t &rawY) const;
	void waitForTap(int32_t &rawX, int32_t &rawY) const;

	Preferences prefs;

	bool calibrated = false;
	int32_t xMin = 0, xMax = 4095, yMin = 0, yMax = 4095;
};
