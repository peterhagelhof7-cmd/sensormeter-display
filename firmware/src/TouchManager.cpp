#include "TouchManager.h"

// Standard-XPT2046-Kommandobytes fuer Einzel-Kanal-Messung (12 Bit,
// single-ended). Zuordnung X/Y kann je nach Verdrahtung vertauscht/gespiegelt
// sein - wird ueber die Zwei-Punkt-Kalibrierung ausgeglichen.
static constexpr uint8_t kCmdReadX = 0xD0;
static constexpr uint8_t kCmdReadY = 0x90;

void TouchManager::begin() {
	pinMode(TOUCH_PIN_CLK, OUTPUT);
	pinMode(TOUCH_PIN_DIN, OUTPUT);
	pinMode(TOUCH_PIN_CS, OUTPUT);
	pinMode(TOUCH_PIN_OUT, INPUT);
	pinMode(TOUCH_PIN_IRQ, INPUT);

	digitalWrite(TOUCH_PIN_CS, HIGH);
	digitalWrite(TOUCH_PIN_CLK, LOW);

	loadCalibration();
}

void TouchManager::loadCalibration() {
	prefs.begin("touch", true);
	calibrated = prefs.getBool("calibrated", false);
	xMin = prefs.getInt("xMin", 0);
	xMax = prefs.getInt("xMax", 4095);
	yMin = prefs.getInt("yMin", 0);
	yMax = prefs.getInt("yMax", 4095);
	prefs.end();
}

void TouchManager::saveCalibration() {
	prefs.begin("touch", false);
	prefs.putBool("calibrated", true);
	prefs.putInt("xMin", xMin);
	prefs.putInt("xMax", xMax);
	prefs.putInt("yMin", yMin);
	prefs.putInt("yMax", yMax);
	prefs.end();
	calibrated = true;
}

bool TouchManager::touched() const {
	// TP_IRQ ist Low-aktiv, solange der Bildschirm beruehrt wird.
	return digitalRead(TOUCH_PIN_IRQ) == LOW;
}

uint16_t TouchManager::readChannel(uint8_t command) const {
	digitalWrite(TOUCH_PIN_CS, LOW);

	for (int8_t i = 7; i >= 0; i--) {
		digitalWrite(TOUCH_PIN_DIN, (command >> i) & 0x01);
		digitalWrite(TOUCH_PIN_CLK, HIGH);
		delayMicroseconds(2);
		digitalWrite(TOUCH_PIN_CLK, LOW);
		delayMicroseconds(2);
	}

	uint16_t result = 0;
	for (int8_t i = 0; i < 12; i++) {
		digitalWrite(TOUCH_PIN_CLK, HIGH);
		delayMicroseconds(2);
		digitalWrite(TOUCH_PIN_CLK, LOW);
		delayMicroseconds(2);
		result <<= 1;
		if (digitalRead(TOUCH_PIN_OUT)) {
			result |= 0x01;
		}
	}

	digitalWrite(TOUCH_PIN_CS, HIGH);
	return result;
}

bool TouchManager::readRaw(int32_t &rawX, int32_t &rawY) const {
	if (!touched()) {
		return false;
	}
	// Vertauscht (Hardware-Befund): Kommando 0xD0 liefert auf dieser
	// Verkabelung tatsaechlich die vertikale Position, 0x90 die horizontale
	// - siehe docs/entscheidungen.md. Deshalb hier ueber Kreuz zugewiesen,
	// statt die Kommando-Konstanten umzubenennen (kCmdReadX/Y-Namen bleiben
	// an den XPT2046-Standardnamen, nicht an dieser Verkabelung).
	rawX = readChannel(kCmdReadY);
	rawY = readChannel(kCmdReadX);
	return touched(); // erneut pruefen, um Ausreisser beim Loslassen zu verwerfen
}

void TouchManager::waitForTap(int32_t &rawX, int32_t &rawY) const {
	// Nur fuer die Kalibrierung genutzt (nicht fuer read() im laufenden
	// Betrieb): mittelt mehrere Samples waehrend anhaltender Beruehrung statt
	// nur den allerersten, moeglicherweise noch verrauschten Kontakt zu
	// nehmen. (Die eigentliche Ursache fuer die zunaechst kaputte 4-Punkt-
	// Kalibrierung war ein vertauschter X/Y-Kanal in readRaw(), nicht fehlende
	// Mittelung - siehe docs/entscheidungen.md. Die Mittelung bleibt trotzdem
	// als zusaetzliche Robustheit gegen einzelne Ausreisser bestehen.)
	constexpr int kSamples = 8;
	constexpr int kMinSamples = 4;

	for (;;) {
		while (!readRaw(rawX, rawY)) {
			delay(20);
		}
		delay(60); // Setzzeit

		int64_t sumX = 0, sumY = 0;
		int count = 0;
		for (int i = 0; i < kSamples; i++) {
			int32_t sx, sy;
			if (readRaw(sx, sy)) {
				sumX += sx;
				sumY += sy;
				count++;
			}
			delay(10);
		}

		if (count >= kMinSamples) {
			rawX = static_cast<int32_t>(sumX / count);
			rawY = static_cast<int32_t>(sumY / count);
			break;
		}
		// Beruehrung war kuerzer als die Setzzeit+Sampling-Dauer (~140ms) -
		// verwerfen und auf einen neuen, laenger gehaltenen Tipp warten,
		// statt den unverzoegerten, potenziell verrauschten Erstwert zu
		// uebernehmen.
	}

	delay(300); // Entprellen
	int32_t dummyX, dummyY;
	while (readRaw(dummyX, dummyY)) {
		delay(20);
	}
}

namespace {
struct CalibTarget {
	int16_t x, y;
	const char *label;
};
} // namespace

void TouchManager::runCalibration(DisplayManager &display) {
	TFT_eSPI &tft = display.raw();

	// Vier Ecken, aber bewusst um kCalibMarginX/Y nach innen versetzt statt
	// exakt auf der Kante - siehe Kommentar in TouchManager.h.
	const CalibTarget targets[4] = {
		{kCalibMarginX, kCalibMarginY, "oben-links"},
		{static_cast<int16_t>(DisplayManager::kScreenWidth - kCalibMarginX), kCalibMarginY, "oben-rechts"},
		{kCalibMarginX, static_cast<int16_t>(DisplayManager::kScreenHeight - kCalibMarginY), "unten-links"},
		{static_cast<int16_t>(DisplayManager::kScreenWidth - kCalibMarginX),
		 static_cast<int16_t>(DisplayManager::kScreenHeight - kCalibMarginY), "unten-rechts"},
	};

	int32_t rawX[4], rawY[4];
	for (int i = 0; i < 4; i++) {
		tft.fillScreen(TFT_WHITE);
		tft.setTextColor(TFT_BLACK, TFT_WHITE);
		tft.setTextDatum(TL_DATUM);
		tft.setTextFont(2);
		tft.drawString(String("Kalibrierung: ") + targets[i].label + " antippen und halten", 10, 10);
		// Fadenkreuz statt 4px-Punkt (Hardware-Befund: der kleine Punkt war auf
		// dem Display schwer praezise zu treffen, was zu eng beieinander
		// liegenden Rohwerten ueber alle vier Ecken fuehrte).
		constexpr int16_t kCrosshairArm = 12;
		tft.drawFastHLine(targets[i].x - kCrosshairArm, targets[i].y, kCrosshairArm * 2, TFT_RED);
		tft.drawFastVLine(targets[i].x, targets[i].y - kCrosshairArm, kCrosshairArm * 2, TFT_RED);
		tft.drawCircle(targets[i].x, targets[i].y, 6, TFT_RED);

		waitForTap(rawX[i], rawY[i]);
	}

	// xMin/xMax bzw. yMin/yMax je aus zwei Tipps gemittelt (z.B. xMin aus
	// oben-links + unten-links) - gleicht einen einzelnen ungenauen Tipp
	// besser aus als eine Zwei-Punkt-Kalibrierung.
	xMin = (rawX[0] + rawX[2]) / 2; // oben-links + unten-links
	xMax = (rawX[1] + rawX[3]) / 2; // oben-rechts + unten-rechts
	yMin = (rawY[0] + rawY[1]) / 2; // oben-links + oben-rechts
	yMax = (rawY[2] + rawY[3]) / 2; // unten-links + unten-rechts
	saveCalibration();

	tft.fillScreen(TFT_WHITE);
	tft.drawString("Kalibrierung gespeichert.", 10, 10);
	delay(800);
}

int16_t TouchManager::mapAxis(int32_t raw, int32_t rawMin, int32_t rawMax, int16_t margin,
                               int16_t screenSize) const {
	if (rawMax == rawMin) {
		return 0;
	}
	// rawMin/rawMax entsprechen den bei der Kalibrierung angetippten,
	// nach innen versetzten Zielen (margin..screenSize-1-margin) - hier
	// linear auf den vollen Bildschirmbereich (0..screenSize-1) extrapoliert,
	// damit Tipps bis an die Kante trotzdem 0 bzw. screenSize-1 ergeben.
	int32_t span = screenSize - 1 - 2 * margin;
	int32_t v = margin + (raw - rawMin) * span / (rawMax - rawMin);
	if (v < 0) v = 0;
	if (v > screenSize - 1) v = screenSize - 1;
	return static_cast<int16_t>(v);
}

bool TouchManager::read(int16_t &screenX, int16_t &screenY) {
	int32_t rawX, rawY;
	if (!readRaw(rawX, rawY)) {
		return false;
	}
	screenX = mapAxis(rawX, xMin, xMax, kCalibMarginX, DisplayManager::kScreenWidth);
	screenY = mapAxis(rawY, yMin, yMax, kCalibMarginY, DisplayManager::kScreenHeight);
	return true;
}
