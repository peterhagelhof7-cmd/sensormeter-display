#include <Arduino.h>

#include "DisplayManager.h"
#include "TouchManager.h"

DisplayManager display;
TouchManager touch;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot (P0)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P0: Display + Touch");

	touch.begin();
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

	display.drawBootScreen("Sensormeter Display", "Touch bereit - antippen zum Testen");
}

void loop() {
	int16_t x, y;
	if (touch.read(x, y)) {
		TFT_eSPI &tft = display.raw();
		tft.fillCircle(x, y, 3, TFT_RED);
		Serial.printf("Touch: x=%d y=%d\n", x, y);
	}
	delay(20);
}
