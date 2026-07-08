#include "SensormeterClient.h"

namespace {
constexpr const char *kOidSensor1Temp = "1.3.6.1.4.1.99999.3.2.0";
constexpr const char *kOidSensor1Hum = "1.3.6.1.4.1.99999.3.3.0";
}

bool SensormeterClient::update(const String &host, const String &community) {
	configured = host.length() > 0;
	if (!configured) {
		valid = false;
		return false;
	}

	uint32_t now = millis();
	if (lastPollMs != 0 && (now - lastPollMs) < kPollIntervalMs) {
		return false;
	}
	lastPollMs = now;

	int32_t temp10 = 0, hum10 = 0;
	bool okTemp = snmp.getInteger(host, community, kOidSensor1Temp, temp10);
	bool okHum = okTemp && snmp.getInteger(host, community, kOidSensor1Hum, hum10);

	if (okTemp && okHum) {
		tempC = temp10 / 10.0f;
		humidityPct = hum10 / 10.0f;
		valid = true;
	}
	// bei Fehlschlag: letzter gueltiger Wert bleibt bestehen (wie SensorManager)
	return true;
}
