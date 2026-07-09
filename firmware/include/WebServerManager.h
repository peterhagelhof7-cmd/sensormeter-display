#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "SettingsManager.h"
#include "BacklightManager.h"
#include "OtaManager.h"
#include "WlanManager.h"
#include "SensormeterManager.h"
#include "SensorManager.h"
#include "PingManager.h"
#include "GraphManager.h"

// Webserver mit zwei Seiten (siehe docs/entscheidungen.md):
// - "/" : oeffentliches, NICHT passwortgeschuetztes Status-Dashboard
//   (Nutzerwunsch: alle aktuellen Werte auf einen Blick, Auto-Refresh 30s,
//   kein Login noetig) - rein lesend, keine Formulare.
// - "/settings" : der bisherige Einstellungs-Bereich (Name, Betriebsmodus,
//   Helligkeit, Sensormeter-/Ping-Ziele, Warnschwellwerte, Netzwerk,
//   OTA-Update), weiterhin per HTTP-Basic-Auth geschuetzt.
// Async (ESPAsyncWebServer/AsyncTCP), damit HTTP-Anfragen den Hauptloop
// (Touch, Sensor-Polling, Ping, Snake) nicht blockieren.
class WebServerManager {
public:
	// sensormeterManager/sensor/pingManager/graph: nur lesend -
	// sensormeterManager fuer die PRO-Sensor-Erkennung in der Schwellwert-
	// Tabelle UND fuer die Sensormeter-Anzeige im Dashboard; sensor/
	// pingManager/graph ausschliesslich fuer die aktuellen Werte bzw. den
	// SVG-Verlaufsgraph im Dashboard.
	WebServerManager(SettingsManager &settings, BacklightManager &backlight, OtaManager &ota, WlanManager &wlan,
	                  const SensormeterManager &sensormeterManager, const SensorManager &sensor,
	                  const PingManager &pingManager, const GraphManager &graph);

	void begin();

private:
	bool checkAuth(AsyncWebServerRequest *request) const;
	// saved: zeigt einen kurzen "Gespeichert"-Hinweis oben auf der Seite -
	// gesetzt, wenn der aufrufende Redirect "?saved=1" mitgibt.
	String buildSettingsPage(bool saved) const;
	String buildDashboardPage() const;
	String sharedCss() const;
	// linkHref leer = kein Link-Badge (nicht gebraucht, wenn eine Seite
	// eh nur die andere Richtung anbieten wuerde).
	String bannerHtml(const String &eyebrow, const String &subLine, const String &linkHref,
	                   const String &linkLabel) const;
	String dataSourceOptions(DataSource selected) const;
	String dhtGraphSvg() const;
	String wifiBarsHtml(int8_t bars) const;

	void handleSave(AsyncWebServerRequest *request);
	void handlePingAdd(AsyncWebServerRequest *request);
	void handlePingRemove(AsyncWebServerRequest *request);
	void handleSensormeterAdd(AsyncWebServerRequest *request);
	void handleSensormeterRemove(AsyncWebServerRequest *request);
	void handleSensormeterThresholds(AsyncWebServerRequest *request);
	void handlePingThreshold(AsyncWebServerRequest *request);
	void handleNetworkSave(AsyncWebServerRequest *request);
	void handleOtaUploadChunk(AsyncWebServerRequest *request, const String &filename, size_t index,
	                           uint8_t *data, size_t len, bool final);
	void handleOtaUploadComplete(AsyncWebServerRequest *request);

	SettingsManager &settings_;
	BacklightManager &backlight_;
	OtaManager &ota_;
	WlanManager &wlan_;
	const SensormeterManager &sensormeterManager_;
	const SensorManager &sensor_;
	const PingManager &pingManager_;
	const GraphManager &graph_;
	AsyncWebServer server_{80};

	bool otaInProgress_ = false;
	bool otaSuccess_ = false;
};
