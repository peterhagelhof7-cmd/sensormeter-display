#pragma once

#include <Arduino.h>

#include "SnmpClient.h"

// Fragt das Sensormeter-Projekt (WT32-ETH01, SNMP v1) alle 30s ab, sofern
// eine Ziel-IP konfiguriert ist. OIDs siehe lastenheft.txt Abschnitt 7.3;
// Temperatur/Luftfeuchte sind dort als INTEGER x10 kodiert (bestaetigt im
// Quellcode des Sensormeter-Projekts, SNMPManager.cpp).
class SensormeterClient {
public:
	static constexpr uint32_t kPollIntervalMs = 30000;

	// Liefert true, wenn in diesem Aufruf tatsaechlich abgefragt wurde
	// (auch bei Fehlschlag) - fuer die Aufrufer-seitige Redraw-Entscheidung.
	bool update(const String &host, const String &community);

	bool isConfigured() const { return configured; }
	bool hasValidReading() const { return valid; }
	float temperatureC() const { return tempC; }
	float humidityPercent() const { return humidityPct; }

private:
	SnmpClient snmp;
	uint32_t lastPollMs = 0;
	bool configured = false;
	bool valid = false;
	float tempC = 0.0f;
	float humidityPct = 0.0f;
};
