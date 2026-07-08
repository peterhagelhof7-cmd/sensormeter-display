#include "SensormeterManager.h"

namespace {
constexpr const char *kOidSystemName = "1.3.6.1.4.1.99999.1.1.0";
constexpr const char *kOidSystemType = "1.3.6.1.4.1.99999.1.3.0";
constexpr const char *kOidSensor1Name = "1.3.6.1.4.1.99999.3.1.0";
constexpr const char *kOidSensor1Temp = "1.3.6.1.4.1.99999.3.2.0";
constexpr const char *kOidSensor1Hum = "1.3.6.1.4.1.99999.3.3.0";
constexpr const char *kOidSensor2Name = "1.3.6.1.4.1.99999.4.1.0";
constexpr const char *kOidSensor2Temp = "1.3.6.1.4.1.99999.4.2.0";
constexpr const char *kOidSensor2Hum = "1.3.6.1.4.1.99999.4.3.0";
} // namespace

void SensormeterManager::syncTargets(const SettingsManager &settings) {
	size_t newCount = settings.sensormeterTargetCount();
	if (newCount > kMaxTargets) newCount = kMaxTargets;

	for (size_t i = 0; i < newCount; i++) {
		String ip = settings.sensormeterTargetIp(i);
		if (targets_[i].ip != ip) {
			targets_[i] = Target();
			targets_[i].ip = ip;
		}
	}
	for (size_t i = newCount; i < kMaxTargets; i++) {
		if (!targets_[i].ip.isEmpty()) {
			targets_[i] = Target();
		}
	}
	count_ = newCount;
	if (nextIndex_ >= count_) nextIndex_ = 0;
}

void SensormeterManager::resolveIdentity(Target &t, const String &community) {
	// Einmalig pro Ziel (nicht auf einzelne Ticks aufgeteilt): passiert nur
	// beim erstmaligen Hinzufuegen/Aendern eines Ziels, nicht im laufenden
	// Betrieb - ein etwas laengerer Blockierzeitraum bei einem gerade neu
	// eingetragenen, noch nicht erreichbaren Ziel ist dafuer akzeptabel
	// (Nutzer sitzt ohnehin gerade in den Einstellungen).
	String name, type;
	bool okName = snmp_.getString(t.ip, community, kOidSystemName, name);
	bool okType = snmp_.getString(t.ip, community, kOidSystemType, type);
	if (!okName || !okType) {
		return; // naechster Round-Robin-Durchlauf versucht es erneut
	}
	t.systemName = name;
	t.isPro = (type == "Sensormeter PRO");

	String s1;
	bool okS1 = snmp_.getString(t.ip, community, kOidSensor1Name, s1);
	if (!okS1) {
		return;
	}
	t.sensorName[0] = s1;

	if (t.isPro) {
		String s2;
		if (!snmp_.getString(t.ip, community, kOidSensor2Name, s2)) {
			return;
		}
		t.sensorName[1] = s2;
	}
	t.identityResolved = true;
}

void SensormeterManager::refreshReadings(Target &t, const String &community) {
	int32_t temp10 = 0, hum10 = 0;
	bool okTemp = snmp_.getInteger(t.ip, community, kOidSensor1Temp, temp10);
	bool okHum = okTemp && snmp_.getInteger(t.ip, community, kOidSensor1Hum, hum10);
	if (okTemp && okHum) {
		t.tempC[0] = temp10 / 10.0f;
		t.humidityPct[0] = hum10 / 10.0f;
		t.valid[0] = true;
	}

	if (t.isPro) {
		int32_t temp2_10 = 0, hum2_10 = 0;
		bool okTemp2 = snmp_.getInteger(t.ip, community, kOidSensor2Temp, temp2_10);
		bool okHum2 = okTemp2 && snmp_.getInteger(t.ip, community, kOidSensor2Hum, hum2_10);
		if (okTemp2 && okHum2) {
			t.tempC[1] = temp2_10 / 10.0f;
			t.humidityPct[1] = hum2_10 / 10.0f;
			t.valid[1] = true;
		}
	}
}

bool SensormeterManager::update(const SettingsManager &settings) {
	syncTargets(settings);
	if (count_ == 0) {
		return false;
	}

	uint32_t now = millis();
	if (lastCheckMs_ != 0 && (now - lastCheckMs_) < kCheckIntervalMs) {
		return false;
	}
	lastCheckMs_ = now;

	String community = settings.sensormeterCommunity();
	Target &t = targets_[nextIndex_];
	if (!t.identityResolved) {
		resolveIdentity(t, community);
	} else {
		refreshReadings(t, community);
	}
	nextIndex_ = (nextIndex_ + 1) % count_;
	return true;
}

bool SensormeterManager::isResolved(size_t i) const {
	return (i < count_) && targets_[i].identityResolved;
}

bool SensormeterManager::isPro(size_t i) const {
	return (i < count_) && targets_[i].isPro;
}

String SensormeterManager::systemName(size_t i) const {
	return (i < count_) ? targets_[i].systemName : String();
}

String SensormeterManager::sensorName(size_t i, uint8_t sensorIndex) const {
	return (i < count_ && sensorIndex < 2) ? targets_[i].sensorName[sensorIndex] : String();
}

bool SensormeterManager::sensorValid(size_t i, uint8_t sensorIndex) const {
	return (i < count_ && sensorIndex < 2) && targets_[i].valid[sensorIndex];
}

float SensormeterManager::sensorTempC(size_t i, uint8_t sensorIndex) const {
	return (i < count_ && sensorIndex < 2) ? targets_[i].tempC[sensorIndex] : 0.0f;
}

float SensormeterManager::sensorHumidityPct(size_t i, uint8_t sensorIndex) const {
	return (i < count_ && sensorIndex < 2) ? targets_[i].humidityPct[sensorIndex] : 0.0f;
}
