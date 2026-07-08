#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "SettingsManager.h"
#include "BacklightManager.h"
#include "OtaManager.h"
#include "WlanManager.h"

// Schlanker Einstellungs-Webserver (lastenheft.txt Abschnitt 8,
// Zusatzfunktion ueber die urspruengliche Beschreibung.txt hinaus): Name,
// Betriebsmodus, Helligkeit, Sensormeter-Ziel, Ping-Ziele setzen, plus
// lokales OTA-Update per .bin-Upload. Async (ESPAsyncWebServer/AsyncTCP),
// damit HTTP-Anfragen den Hauptloop (Touch, Sensor-Polling, Ping, Snake)
// nicht blockieren - siehe docs/entscheidungen.md.
//
// Kein REST-API/JSON noetig (anders als das Sensormeter-Projekt) - hier
// geht es nur um ein Einstellungsformular, kein Anzeige-Dashboard.
class WebServerManager {
public:
	WebServerManager(SettingsManager &settings, BacklightManager &backlight, OtaManager &ota, WlanManager &wlan);

	void begin();

private:
	bool checkAuth(AsyncWebServerRequest *request) const;
	String buildPage() const;
	String dataSourceOptions(DataSource selected) const;

	void handleSave(AsyncWebServerRequest *request);
	void handlePingAdd(AsyncWebServerRequest *request);
	void handlePingRemove(AsyncWebServerRequest *request);
	void handleSensormeterAdd(AsyncWebServerRequest *request);
	void handleSensormeterRemove(AsyncWebServerRequest *request);
	void handleNetworkSave(AsyncWebServerRequest *request);
	void handleOtaUploadChunk(AsyncWebServerRequest *request, const String &filename, size_t index,
	                           uint8_t *data, size_t len, bool final);
	void handleOtaUploadComplete(AsyncWebServerRequest *request);

	SettingsManager &settings_;
	BacklightManager &backlight_;
	OtaManager &ota_;
	WlanManager &wlan_;
	AsyncWebServer server_{80};

	bool otaInProgress_ = false;
	bool otaSuccess_ = false;
};
