#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot (P1)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P1: WLAN-Ersteinrichtung");

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

	display.drawBootScreen("Sensormeter Display", "WLAN verbunden");
}

void loop() {
	int16_t x, y;
	if (touch.read(x, y)) {
		TFT_eSPI &tft = display.raw();
		tft.fillCircle(x, y, 3, TFT_RED);
	}
	delay(20);
}
