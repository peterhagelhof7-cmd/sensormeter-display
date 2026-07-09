#pragma once

#include <Arduino.h>

#include "SettingsManager.h"

// Kontinuierlicher Ping an google.com (lastenheft.txt Abschnitt 7.4 + 8/9)
// plus bis zu 5 zusaetzliche, ueber die Einstellungen konfigurierte Ziele.
// Um die Touch-Reaktion nicht durch mehrere Timeouts pro Zyklus zu
// blockieren, wird pro update()-Aufruf hoechstens EIN Google-Ping UND EIN
// Ziel-Ping (round-robin) ausgefuehrt, nicht alle auf einmal - siehe
// docs/entscheidungen.md.
class PingManager {
public:
	static constexpr uint32_t kCheckIntervalMs = 2000;
	static constexpr uint32_t kFailureAlertMs = 60000;
	static constexpr size_t kMaxTargets = 5;

	void begin();
	// Liefert true, wenn in diesem Aufruf tatsaechlich gepingt wurde.
	bool update(const SettingsManager &settings);

	bool hasGoogleReading() const { return recentCount > 0; }
	float googleAverageLatencyMs() const;
	bool isFailingOver1Min() const;

	size_t targetCount() const { return numTargets; }
	String targetIp(size_t i) const { return (i < numTargets) ? targets[i].ip : String(); }
	bool targetOk(size_t i) const { return (i < numTargets) ? targets[i].ok : false; }
	bool targetChecked(size_t i) const { return (i < numTargets) ? targets[i].checked : false; }
	// Letzte erfolgreich gemessene Latenz (ms). Bleibt bei einem
	// fehlgeschlagenen Ping auf dem zuletzt erfolgreichen Wert stehen -
	// targetHasLatency() unterscheidet "noch nie erfolgreich" von "0ms".
	float targetLatencyMs(size_t i) const { return (i < numTargets) ? targets[i].lastLatencyMs : 0.0f; }
	bool targetHasLatency(size_t i) const { return (i < numTargets) ? targets[i].hasLatency : false; }

private:
	void pingGoogle();
	void pingNextTarget(const SettingsManager &settings);

	struct TargetState {
		String ip;
		bool ok = false;
		bool checked = false;
		float lastLatencyMs = 0.0f;
		bool hasLatency = false;
	};

	uint32_t lastCheckMs = 0;
	float recentLatencies[5] = {0, 0, 0, 0, 0};
	size_t recentCount = 0;
	size_t recentWriteIndex = 0;

	uint32_t failureStreakStartMs = 0;

	TargetState targets[kMaxTargets];
	size_t numTargets = 0;
	size_t nextTargetIndex = 0;
};
