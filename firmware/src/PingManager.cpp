#include "PingManager.h"

#include <ESP32Ping.h>

void PingManager::begin() {
	// kein spezieller Init-Schritt noetig (ESP32Ping braucht keinen begin())
}

void PingManager::pingGoogle() {
	bool ok = Ping.ping("google.com", 1);
	if (ok) {
		float latency = static_cast<float>(Ping.averageTime());
		recentLatencies[recentWriteIndex] = latency;
		recentWriteIndex = (recentWriteIndex + 1) % 5;
		if (recentCount < 5) recentCount++;
		failureStreakStartMs = 0;
	} else if (failureStreakStartMs == 0) {
		failureStreakStartMs = millis();
	}
}

void PingManager::pingNextTarget(const SettingsManager &settings) {
	numTargets = settings.pingTargetCount();
	if (numTargets == 0) {
		return;
	}
	if (nextTargetIndex >= numTargets) {
		nextTargetIndex = 0;
	}

	size_t i = nextTargetIndex;
	String ip = settings.pingTargetIp(i);
	targets[i].ip = ip;
	targets[i].ok = Ping.ping(ip.c_str(), 1);
	targets[i].checked = true;
	if (targets[i].ok) {
		targets[i].lastLatencyMs = static_cast<float>(Ping.averageTime());
		targets[i].hasLatency = true;
	}

	nextTargetIndex = (nextTargetIndex + 1) % numTargets;
}

bool PingManager::update(const SettingsManager &settings) {
	uint32_t now = millis();
	if (now - lastCheckMs < kCheckIntervalMs && lastCheckMs != 0) {
		return false;
	}
	lastCheckMs = now;

	pingGoogle();
	pingNextTarget(settings);
	return true;
}

float PingManager::googleAverageLatencyMs() const {
	if (recentCount == 0) {
		return 0.0f;
	}
	float sum = 0.0f;
	for (size_t i = 0; i < recentCount; i++) {
		sum += recentLatencies[i];
	}
	return sum / static_cast<float>(recentCount);
}

bool PingManager::isFailingOver1Min() const {
	return failureStreakStartMs != 0 && (millis() - failureStreakStartMs) > kFailureAlertMs;
}
