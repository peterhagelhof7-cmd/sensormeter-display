#include "WlanManager.h"

#include <algorithm>

void WlanManager::begin() {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
}

std::vector<WlanManager::NetworkInfo> WlanManager::scan(uint8_t maxResults) {
	std::vector<NetworkInfo> results;

	int16_t count = WiFi.scanNetworks();
	if (count <= 0) {
		return results;
	}

	for (int16_t i = 0; i < count; i++) {
		String ssid = WiFi.SSID(i);
		if (ssid.isEmpty()) {
			continue;
		}
		int32_t rssi = WiFi.RSSI(i);

		bool merged = false;
		for (auto &existing : results) {
			if (existing.ssid == ssid) {
				if (rssi > existing.rssi) {
					existing.rssi = rssi;
				}
				merged = true;
				break;
			}
		}
		if (!merged) {
			NetworkInfo info;
			info.ssid = ssid;
			info.rssi = rssi;
			info.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
			results.push_back(info);
		}
	}

	std::sort(results.begin(), results.end(), [](const NetworkInfo &a, const NetworkInfo &b) {
		return a.rssi > b.rssi;
	});

	if (results.size() > maxResults) {
		results.resize(maxResults);
	}

	WiFi.scanDelete();
	return results;
}

bool WlanManager::hasCredentials() {
	prefs.begin("wifi", true);
	bool has = prefs.isKey("ssid");
	prefs.end();
	return has;
}

bool WlanManager::loadCredentials(String &ssid, String &psk) {
	prefs.begin("wifi", true);
	if (!prefs.isKey("ssid")) {
		prefs.end();
		return false;
	}
	ssid = prefs.getString("ssid", "");
	psk = prefs.getString("psk", "");
	prefs.end();
	return !ssid.isEmpty();
}

void WlanManager::saveCredentials(const String &ssid, const String &psk) {
	prefs.begin("wifi", false);
	prefs.putString("ssid", ssid);
	prefs.putString("psk", psk);
	prefs.end();
}

bool WlanManager::connect(const String &ssid, const String &psk, uint32_t timeoutMs) {
	WiFi.disconnect();
	delay(100);
	WiFi.begin(ssid.c_str(), psk.c_str());

	uint32_t start = millis();
	while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
		delay(200);
	}
	return WiFi.status() == WL_CONNECTED;
}

bool WlanManager::autoConnect(uint32_t timeoutMs) {
	String ssid, psk;
	if (!loadCredentials(ssid, psk)) {
		return false;
	}
	return connect(ssid, psk, timeoutMs);
}

int8_t WlanManager::signalBars() const {
	if (!isConnected()) {
		return -1; // kein WLAN - Symbol soll blinken
	}
	int32_t rssi = WiFi.RSSI();
	if (rssi >= -60) return 3;
	if (rssi >= -75) return 2;
	return 1;
}
