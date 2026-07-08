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
// Ergebnis) - auf echter Hardware verifiziert (siehe docs/entscheidungen.md).
class TouchManager {
public:
	// Kalibrierziele liegen bewusst nicht auf den Bildschirmkanten selbst,
	// sondern etwas nach innen versetzt (Feedback nach erstem Hardware-Test:
	// Ecken exakt auf der Kante waren schlecht/ungenau antippbar). Die
	// tatsaechlichen Bildschirmkanten werden trotzdem ueber lineare
	// Extrapolation abgedeckt, siehe mapAxis().
	static constexpr int16_t kCalibMarginX = 40;
	static constexpr int16_t kCalibMarginY = 32;

	void begin();

	bool isCalibrated() const { return calibrated; }
	// Vier-Punkt-Kalibrierung (alle vier Ecken antippen, siehe kCalibMarginX/Y):
	// robuster gegen einen einzelnen ungenauen Tipp als die fruehere
	// Zwei-Punkt-Variante, da xMin/xMax/yMin/yMax jeweils aus zwei Tipps
	// gemittelt werden.
	void runCalibration(DisplayManager &display);

	// true, wenn aktuell beruehrt; screenX/screenY in Bildschirmkoordinaten
	// (0..319 x, 0..239 y), nur gueltig wenn isCalibrated().
	bool read(int16_t &screenX, int16_t &screenY);

private:
	void loadCalibration();
	void saveCalibration();
	int16_t mapAxis(int32_t raw, int32_t rawMin, int32_t rawMax, int16_t margin, int16_t screenSize) const;

	bool touched() const;
	uint16_t readChannel(uint8_t command) const;
	bool readRaw(int32_t &rawX, int32_t &rawY) const;
	void waitForTap(int32_t &rawX, int32_t &rawY) const;

	Preferences prefs;

	bool calibrated = false;
	int32_t xMin = 0, xMax = 4095, yMin = 0, yMax = 4095;
};
