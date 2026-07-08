#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SensorManager.h"
#include "GraphManager.h"
#include "ClockView.h"
#include "SensormeterClient.h"
#include "SensormeterView.h"
#include "PingManager.h"
#include "PingView.h"
#include "LedManager.h"
#include "StatusBar.h"
#include "TimeSync.h"
#include "Layout.h"
#include "DataSource.h"
#include "SettingsManager.h"
#include "BacklightManager.h"
#include "SettingsUI.h"
#include "UiHelpers.h"
#include "OtaManager.h"
#include "WebServerManager.h"

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;
SensorManager sensor;
GraphManager graph;
ClockView clockView;
SensormeterClient sensormeterClient;
SensormeterView sensormeterView;
PingManager pingManager;
PingView pingView;
LedManager led;
StatusBar statusBar;
SettingsManager settings;
BacklightManager backlight;
SettingsUI settingsUI;
OtaManager ota;
WebServerManager webServer(settings, backlight, ota);

uint32_t lastStatusBarMs = 0;
// 300ms statt z.B. 1000ms, damit das 500ms-Blinken des WLAN-Symbols nicht
// mit der Redraw-Rate aliast (bei exaktem 1000ms-Takt wuerde immer dieselbe
// Blink-Phase abgetastet und das Symbol nie sichtbar blinken).
constexpr uint32_t kStatusBarIntervalMs = 300;

uint32_t slideLastSwitchMs = 0;
size_t slideIndex = 0;
bool contentDirty = true;
uint32_t lastPeriodicRedrawMs = 0;
constexpr uint32_t kPeriodicRedrawIntervalMs = 5000;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P7/P8: Sensormeter + Ping");

	touch.begin();
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

	settings.begin();
	backlight.begin(settings.brightnessPercent());
	led.begin();

	wlan.begin();
	bool connected = false;
	if (wlan.hasCredentials()) {
		display.drawBootScreen("WLAN", "Verbinde mit gespeichertem Netz ...");
		connected = wlan.autoConnect();
	}
	if (!connected) {
		Serial.println("Kein WLAN verbunden - starte Onboarding");
		onboarding.run(display, touch, wlan);
	}
	Serial.print("WLAN verbunden, IP: ");
	Serial.println(WiFi.localIP());

	TimeSync::begin();
	sensor.begin();
	graph.begin();
	pingManager.begin();
	webServer.begin();
}

DataSource currentActiveSource() {
	if (settings.mode() == OperatingMode::Static) {
		return settings.staticSource();
	}

	uint32_t now = millis();
	uint32_t intervalMs = static_cast<uint32_t>(settings.slideIntervalSec()) * 1000UL;
	if (now - slideLastSwitchMs >= intervalMs) {
		slideLastSwitchMs = now;
		slideIndex = (slideIndex + 1) % kAvailableDataSourceCount;
		contentDirty = true;
	}
	return kAvailableDataSources[slideIndex];
}

void loop() {
	// Zahnrad in der Statusleiste antippbar - oeffnet die Einstellungen
	// (blockierend). Erst auf Loslassen warten, damit der modale Dialog
	// nicht denselben, noch gehaltenen Tipp als seinen ersten Tastendruck
	// missversteht.
	int16_t tx, ty;
	bool touchedNow = touch.read(tx, ty);
	if (touchedNow &&
	    UiHelpers::hitRect(tx, ty, StatusBar::kGearHitX, StatusBar::kGearHitY, StatusBar::kGearHitW,
	                        StatusBar::kGearHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		settingsUI.run(display, touch, wlan, settings, backlight, onboarding);
		contentDirty = true;
		lastStatusBarMs = 0;
	}

	bool dhtPolled = sensor.update();
	if (dhtPolled) {
		graph.maybeRecord(sensor.temperatureC(), sensor.humidityPercent());
	}
	bool sensormeterPolled =
	    sensormeterClient.update(settings.sensormeterIp(), settings.sensormeterCommunity());
	bool pingPolled = pingManager.update(settings);

	// Anhaltender Ping-Fehler (>1 Min. an google.com): LED blinkt rot und
	// die gesamte Anzeige wechselt auf roten Hintergrund (lastenheft.txt
	// Abschnitt 9), unabhaengig von der gerade aktiven Datenquelle.
	bool alertActive = pingManager.isFailingOver1Min();
	led.update(alertActive);
	uint16_t bgColor = alertActive ? TFT_RED : TFT_WHITE;

	DataSource activeSource = currentActiveSource();
	bool showBottomBar = !(settings.mode() == OperatingMode::Static && activeSource == DataSource::Uhrzeit);
	int16_t contentBottom = showBottomBar ? Layout::kContentBottom : DisplayManager::kScreenHeight;

	uint32_t now = millis();
	bool periodicDue = (now - lastPeriodicRedrawMs >= kPeriodicRedrawIntervalMs);

	bool sourceJustPolled = (activeSource == DataSource::Dht11 && dhtPolled) ||
	                        (activeSource == DataSource::Sensormeter && sensormeterPolled) ||
	                        ((activeSource == DataSource::Ping || activeSource == DataSource::PingTargets) &&
	                         pingPolled);
	// Alarmzustand kann sich jederzeit aendern (z.B. genau nach 60s Ausfall) -
	// erzwingt in dem Fall ebenfalls ein Neuzeichnen, auch ohne neue Messung.
	static bool lastAlertActive = false;
	bool alertChanged = (alertActive != lastAlertActive);
	lastAlertActive = alertActive;

	if (contentDirty || sourceJustPolled || periodicDue || alertChanged) {
		switch (activeSource) {
			case DataSource::Dht11:
				graph.drawFullScreen(display, sensor.temperatureC(), sensor.humidityPercent(),
				                     sensor.hasValidReading(), bgColor);
				break;
			case DataSource::Uhrzeit:
				clockView.draw(display, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Sensormeter:
				sensormeterView.draw(display, sensormeterClient, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Ping:
				pingView.drawAverage(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::PingTargets:
				pingView.drawTargetList(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
		}
		contentDirty = false;
		lastPeriodicRedrawMs = now;
	}

	if (now - lastStatusBarMs >= kStatusBarIntervalMs) {
		lastStatusBarMs = now;
		statusBar.draw(display, wlan, sensor.hasValidReading(), sensor.temperatureC(),
		               sensor.humidityPercent(), TimeSync::formatTime(), TimeSync::formatDate(),
		               showBottomBar, bgColor);
	}

	delay(20);
}
