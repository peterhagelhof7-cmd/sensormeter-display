#pragma once

#include <Arduino.h>
#include <time.h>

#include "DisplayManager.h"

// DHT11-Datenquelle: aktueller Wert (oberes Drittel) + Temperatur-/
// Feuchte-Verlauf (untere zwei Drittel), siehe lastenheft.txt Abschnitt 7.1.
// Verlauf als Ringpuffer (24 Eintraege a 30 Minuten = 12h) in
// history.csv auf LittleFS, wird beim Boot geladen.
class GraphManager {
public:
	static constexpr size_t kMaxEntries = 24;
	static constexpr uint32_t kRecordIntervalSec = 30UL * 60UL;

	void begin();
	// In loop() aufrufen; legt hoechstens alle 30 Minuten einen neuen
	// Eintrag an - nur wenn eine gueltige Uhrzeit vorliegt (TimeSync).
	void maybeRecord(float tempC, float humidityPct);

	void drawFullScreen(DisplayManager &display, float currentTempC, float currentHumidityPct,
	                     bool sensorValid);

private:
	struct Entry {
		time_t ts;
		int16_t tempC;
		int16_t humidityPct;
	};

	void load();
	void save() const;
	void appendEntry(time_t ts, int16_t tempC, int16_t humidityPct);
	void drawGraph(DisplayManager &display, int16_t x, int16_t y, int16_t w, int16_t h) const;

	Entry entries[kMaxEntries];
	size_t count = 0;
	time_t lastRecordTs = 0;
};
