#pragma once

#include <Arduino.h>

#include "SettingsManager.h"
#include "SnmpClient.h"

// Fragt bis zu SettingsManager::kMaxSensormeterTargets Sensormeter-Ziele
// (WT32-ETH01, SNMP v1) im Round-Robin ab - ein Ziel pro update()-Aufruf,
// analog zu PingManager::pingNextTarget(), damit ein einzelnes nicht
// erreichbares Ziel nicht den ganzen Hauptloop blockiert. OIDs siehe
// lastenheft.txt Abschnitt 7.3 / docs/entscheidungen.md.
//
// Pro Ziel wird zuerst einmalig die Identitaet aufgeloest (Systemname, Typ -
// zur PRO-Erkennung ueber den String "Sensormeter PRO" - und Sensor-1/2-
// Name), danach laufend nur noch die Messwerte aktualisiert. Ein Ziel vom
// Typ "Sensormeter PRO" liefert zwei Sensor-Slides (Sensor 1 + 2), alle
// anderen genau einen - siehe SensormeterView, die diese Slides anzeigt.
class SensormeterManager {
public:
	static constexpr size_t kMaxTargets = SettingsManager::kMaxSensormeterTargets;
	static constexpr uint32_t kCheckIntervalMs = 4000;

	// Liefert true, wenn in diesem Aufruf tatsaechlich ein SNMP-Vorgang
	// stattfand (fuer die Aufrufer-seitige Redraw-Entscheidung).
	bool update(const SettingsManager &settings);

	size_t targetCount() const { return count_; }
	bool isResolved(size_t i) const;
	bool isPro(size_t i) const;
	String systemName(size_t i) const;
	// sensorIndex: 0 = Sensor 1 (immer vorhanden), 1 = Sensor 2 (nur wenn isPro()).
	String sensorName(size_t i, uint8_t sensorIndex) const;
	bool sensorValid(size_t i, uint8_t sensorIndex) const;
	float sensorTempC(size_t i, uint8_t sensorIndex) const;
	float sensorHumidityPct(size_t i, uint8_t sensorIndex) const;

private:
	struct Target {
		String ip;
		bool identityResolved = false;
		String systemName = "Sensormeter";
		bool isPro = false;
		String sensorName[2] = {"Intern", ""};
		bool valid[2] = {false, false};
		float tempC[2] = {0, 0};
		float humidityPct[2] = {0, 0};
	};

	// Gleicht targets_ mit den in den Einstellungen konfigurierten IPs ab -
	// bei Aenderung (neues/entferntes/geaendertes Ziel) wird der betroffene
	// Slot zurueckgesetzt, damit seine Identitaet neu aufgeloest wird.
	void syncTargets(const SettingsManager &settings);
	void resolveIdentity(Target &t, const String &community);
	void refreshReadings(Target &t, const String &community);

	Target targets_[kMaxTargets];
	size_t count_ = 0;
	size_t nextIndex_ = 0;
	uint32_t lastCheckMs_ = 0;
	SnmpClient snmp_;
};
