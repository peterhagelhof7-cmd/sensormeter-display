#pragma once

#include "DisplayManager.h"
#include "WlanManager.h"

// Permanente Statusleisten auf den beiden langen Bildschirmkanten (siehe
// Layout.h): oben Zahnrad + Info-Symbol + WLAN-Empfang + DHT11-Werte, unten
// Uhrzeit/Datum.
// lastenheft.txt Abschnitt 4 verlangt "hellgrau" fuer die Symbolfarbe;
// TFT_LIGHTGREY war auf dem echten Panel zu kontrastarm, TFT_DARKGREY
// gewaehlt (siehe docs/entscheidungen.md fuer die Fehlersuche - eigentliche
// Ursache eines frueheren "unsichtbar"-Befunds war eine globale
// Farbinvertierung, siehe DisplayManager).
class StatusBar {
public:
	// Trefferbereich des Zahnrad-Symbols, fuer den Touch-Test in main.cpp
	// (oeffnet SettingsUI). Zentral hier definiert, damit Zeichnung und
	// Treffertest nicht auseinanderlaufen koennen.
	static constexpr int16_t kGearHitX = 2;
	static constexpr int16_t kGearHitY = 2;
	static constexpr int16_t kGearHitW = 32;
	static constexpr int16_t kGearHitH = 32;

	// Info-Symbol ("i"): oeffnet InfoUI (Systemname/IP/DHCP-Static). Rechts
	// neben dem WLAN-Symbol platziert (das reicht bis x=44+2*8+6=66), mit
	// etwas Abstand.
	static constexpr int16_t kInfoHitX = 76;
	static constexpr int16_t kInfoHitY = 2;
	static constexpr int16_t kInfoHitW = 32;
	static constexpr int16_t kInfoHitH = 32;

	// showBottomBar=false laesst die untere Leiste komplett weg (Static-Modus
	// mit Datenquelle "Uhrzeit" - siehe lastenheft.txt Abschnitt 6.2).
	void draw(DisplayManager &display, WlanManager &wlan, bool sensorValid, float tempC,
	          float humidityPct, const String &timeHHMM, const String &dateLine,
	          bool showBottomBar = true, uint16_t bgColor = TFT_WHITE);

private:
	void drawGearIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r, uint16_t bgColor) const;
	void drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, int8_t bars) const;
	void drawInfoIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r, uint16_t bgColor) const;

	// Redraw-Cache: draw() wird von main.cpp alle 300ms aufgerufen (siehe dort
	// zur Begruendung des Intervalls), aber ein voller fillRect+Icon-Redraw
	// bei unveraenderten Werten erzeugte sichtbares Flackern (Hardware-Befund
	// nach dem Kontrast-Fix auf schwarze Symbole - vorher mit dem fast
	// unsichtbaren Hellgrau nicht aufgefallen). Nur bei tatsaechlicher
	// Aenderung oder aktivem WLAN-Blinken (bars<0) neu zeichnen.
	bool everDrawn = false;
	int8_t lastBars = -2;
	bool lastSensorValid = false;
	int lastTempRounded = 0;
	int lastHumidityRounded = 0;
	String lastTimeHHMM;
	String lastDateLine;
	bool lastShowBottomBar = true;
	uint16_t lastBgColor = 0;
};
