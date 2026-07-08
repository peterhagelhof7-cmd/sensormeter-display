#pragma once

#include "DisplayManager.h"
#include "WlanManager.h"

// Permanente Statusleisten auf den beiden langen Bildschirmkanten (siehe
// Layout.h): oben Zahnrad + WLAN-Empfang + DHT11-Werte, unten Uhrzeit/Datum.
// Symbolfarbe hellgrau, siehe lastenheft.txt Abschnitt 4.
class StatusBar {
public:
	// Trefferbereich des Zahnrad-Symbols, fuer den Touch-Test in main.cpp
	// (oeffnet SettingsUI). Zentral hier definiert, damit Zeichnung und
	// Treffertest nicht auseinanderlaufen koennen.
	static constexpr int16_t kGearHitX = 2;
	static constexpr int16_t kGearHitY = 2;
	static constexpr int16_t kGearHitW = 32;
	static constexpr int16_t kGearHitH = 32;

	// showBottomBar=false laesst die untere Leiste komplett weg (Static-Modus
	// mit Datenquelle "Uhrzeit" - siehe lastenheft.txt Abschnitt 6.2).
	void draw(DisplayManager &display, WlanManager &wlan, bool sensorValid, float tempC,
	          float humidityPct, const String &timeHHMM, const String &dateLine,
	          bool showBottomBar = true);

private:
	void drawGearIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r) const;
	void drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, int8_t bars) const;
};
