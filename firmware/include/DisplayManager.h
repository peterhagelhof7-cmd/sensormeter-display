#pragma once

#include <TFT_eSPI.h>

// Kapselt die TFT-Ansteuerung (ST7789P3, 240x320, Querbetrieb).
// Systemweite Schriftart: TFT_eSPI Font 7 (7-Segment-Stil) fuer numerische
// Werte (Temperatur, Uhrzeit) - dieser Font kennt nur Ziffern, Doppelpunkt,
// Punkt und Minus. Fuer Text (WLAN-Liste, Tastatur, Wochentag) wird Font 4
// als lesbare Ergaenzung verwendet, da ein reiner 7-Segment-Font keine
// Buchstaben darstellen kann. Siehe docs/entscheidungen.md.
class DisplayManager {
public:
	static constexpr int16_t kScreenWidth = 320;
	static constexpr int16_t kScreenHeight = 240;

	void begin();
	void drawBootScreen(const char *line1, const char *line2);

	TFT_eSPI &raw() { return tft; }

private:
	TFT_eSPI tft;
};
