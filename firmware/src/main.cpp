#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SensorManager.h"
#include "GraphManager.h"
#include "ClockView.h"
#include "StatusBar.h"
#include "TimeSync.h"
#include "Layout.h"
#include "DataSource.h"
#include "SettingsManager.h"
#include "BacklightManager.h"
#include "SettingsUI.h"
#include "UiHelpers.h"

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;
SensorManager sensor;
GraphManager graph;
ClockView clockView;
StatusBar statusBar;
SettingsManager settings;
BacklightManager backlight;
SettingsUI settingsUI;

uint32_t lastStatusBarMs = 0;
// 300ms statt z.B. 1000ms, damit das 500ms-Blinken des WLAN-Symbols nicht
// mit der Redraw-Rate aliast (bei exaktem 1000ms-Takt wuerde immer dieselbe
// Blink-Phase abgetastet und das Symbol nie sichtbar blinken).
constexpr uint32_t kStatusBarIntervalMs = 300;

uint32_t slideLastSwitchMs = 0;
size_t slideIndex = 0;
bool contentDirty = true;
uint32_t lastClockRedrawMs = 0;
constexpr uint32_t kClockRedrawIntervalMs = 5000;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot (P4/P5)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P4/P5: Uhrzeit + Einstellungen");

	touch.begin();
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

	settings.begin();
	backlight.begin(settings.brightnessPercent());

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
	if (touch.read(tx, ty) &&
	    UiHelpers::hitRect(tx, ty, StatusBar::kGearHitX, StatusBar::kGearHitY, StatusBar::kGearHitW,
	                        StatusBar::kGearHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		settingsUI.run(display, touch, wlan, settings, backlight, onboarding);
		contentDirty = true;
		lastStatusBarMs = 0;
	}

	bool polled = sensor.update();
	if (polled) {
		graph.maybeRecord(sensor.temperatureC(), sensor.humidityPercent());
	}

	DataSource activeSource = currentActiveSource();
	bool showBottomBar = !(settings.mode() == OperatingMode::Static && activeSource == DataSource::Uhrzeit);
	int16_t contentBottom = showBottomBar ? Layout::kContentBottom : DisplayManager::kScreenHeight;

	uint32_t now = millis();
	bool clockDue = (activeSource == DataSource::Uhrzeit) && (now - lastClockRedrawMs >= kClockRedrawIntervalMs);

	if (contentDirty || (activeSource == DataSource::Dht11 && polled) || clockDue) {
		if (activeSource == DataSource::Dht11) {
			graph.drawFullScreen(display, sensor.temperatureC(), sensor.humidityPercent(),
			                     sensor.hasValidReading());
		} else {
			clockView.draw(display, Layout::kContentTop, contentBottom);
			lastClockRedrawMs = now;
		}
		contentDirty = false;
	}

	if (now - lastStatusBarMs >= kStatusBarIntervalMs) {
		lastStatusBarMs = now;
		statusBar.draw(display, wlan, sensor.hasValidReading(), sensor.temperatureC(),
		               sensor.humidityPercent(), TimeSync::formatTime(), TimeSync::formatDate(),
		               showBottomBar);
	}

	delay(20);
}
