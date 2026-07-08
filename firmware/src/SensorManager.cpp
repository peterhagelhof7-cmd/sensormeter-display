#include "SensorManager.h"

void SensorManager::begin() {
	dht.begin();
}

bool SensorManager::isPlausible(float tempC, float humidityPct) const {
	if (isnan(tempC) || isnan(humidityPct)) {
		return false;
	}
	// DHT11-Messbereich lt. Hersteller: 0-50 degC, 20-90% rH.
	if (tempC < -10.0f || tempC > 60.0f) {
		return false;
	}
	if (humidityPct < 0.0f || humidityPct > 100.0f) {
		return false;
	}
	return true;
}

bool SensorManager::update() {
	uint32_t now = millis();
	if (now - lastPollMs < kPollIntervalMs && lastPollMs != 0) {
		return false;
	}
	lastPollMs = now;

	float t = dht.readTemperature();
	float h = dht.readHumidity();

	if (isPlausible(t, h)) {
		lastTempC = t;
		lastHumidityPct = h;
		valid = true;
	}
	// bei Implausibilitaet: letzter gueltiger Wert bleibt bestehen
	// (valid wird nur beim allerersten Fehlversuch nicht auf true gesetzt)
	return true;
}
