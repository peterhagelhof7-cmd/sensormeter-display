#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SensorManager.h"
#include "GraphManager.h"
#include "StatusBar.h"
#include "TimeSync.h"
#include "Layout.h"

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;
SensorManager sensor;
GraphManager graph;
StatusBar statusBar;

uint32_t lastStatusBarMs = 0;
// 300ms statt z.B. 1000ms, damit das 500ms-Blinken des WLAN-Symbols nicht
// mit der Redraw-Rate aliast (bei exaktem 1000ms-Takt wuerde immer dieselbe
// Blink-Phase abgetastet und das Symbol nie sichtbar blinken).
constexpr uint32_t kStatusBarIntervalMs = 300;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot (P2/P3)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P2/P3: Statusleiste + DHT11");

	touch.begin();
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

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

void loop() {
	bool polled = sensor.update();
	if (polled) {
		graph.maybeRecord(sensor.temperatureC(), sensor.humidityPercent());
		graph.drawFullScreen(display, sensor.temperatureC(), sensor.humidityPercent(),
		                     sensor.hasValidReading());
	}

	uint32_t now = millis();
	if (now - lastStatusBarMs >= kStatusBarIntervalMs) {
		lastStatusBarMs = now;
		statusBar.draw(display, wlan, sensor.hasValidReading(), sensor.temperatureC(),
		               sensor.humidityPercent(), TimeSync::formatTime(), TimeSync::formatDate());
	}

	delay(20);
}
