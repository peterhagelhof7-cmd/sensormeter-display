#pragma once

#include "DisplayManager.h"
#include "WlanManager.h"

// Permanente Statusleisten auf den beiden langen Bildschirmkanten (siehe
// Layout.h): oben Zahnrad + WLAN-Empfang + DHT11-Werte, unten Uhrzeit/Datum.
// Symbolfarbe hellgrau, siehe lastenheft.txt Abschnitt 4. Zahnrad ist aktuell
// nur dargestellt, Antippen/Einstellungen-Aufruf folgt in P5.
class StatusBar {
public:
	void draw(DisplayManager &display, WlanManager &wlan, bool sensorValid, float tempC,
	          float humidityPct, const String &timeHHMM, const String &dateLine);

private:
	void drawGearIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r) const;
	void drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, int8_t bars) const;
};
