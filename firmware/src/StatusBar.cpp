#include "StatusBar.h"

#include <math.h>

#include "Layout.h"

namespace {
// lastenheft.txt Abschnitt 4 verlangt "hellgrau" - auf dem echten Panel
// (2,8" ST7789P3) aber praktisch nicht von Weiss zu unterscheiden
// (Hardware-Befund, siehe docs/entscheidungen.md). Schwarz gewaehlt fuer
// tatsaechliche Lesbarkeit statt eines technisch spec-treuen, aber
// unsichtbaren Grautons.
constexpr uint16_t kIconColor = TFT_BLACK;
}

void StatusBar::drawGearIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int16_t r) const {
	tft.drawCircle(cx, cy, r, kIconColor);
	tft.drawCircle(cx, cy, r - 3, kIconColor);
	constexpr int8_t kTeeth = 8;
	for (int8_t i = 0; i < kTeeth; i++) {
		float angle = i * (2.0f * PI / kTeeth);
		int16_t x1 = cx + static_cast<int16_t>((r + 1) * cosf(angle));
		int16_t y1 = cy + static_cast<int16_t>((r + 1) * sinf(angle));
		int16_t x2 = cx + static_cast<int16_t>((r + 4) * cosf(angle));
		int16_t y2 = cy + static_cast<int16_t>((r + 4) * sinf(angle));
		tft.drawLine(x1, y1, x2, y2, kIconColor);
	}
}

void StatusBar::drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, int8_t bars) const {
	// bars: -1 = kein WLAN (blinkt), 0-3 = Empfangsstaerke.
	if (bars < 0) {
		bool visible = (millis() / 500) % 2 == 0;
		if (!visible) {
			return;
		}
		bars = 3; // Umriss aller 3 Balken beim Blinken anzeigen
		for (int8_t b = 0; b < 3; b++) {
			int16_t bh = 5 + b * 5;
			tft.drawRect(x + b * 8, y + 16 - bh, 6, bh, kIconColor);
		}
		return;
	}
	for (int8_t b = 0; b < 3; b++) {
		int16_t bh = 5 + b * 5;
		int16_t by = y + 16 - bh;
		if (b < bars) {
			tft.fillRect(x + b * 8, by, 6, bh, kIconColor);
		} else {
			tft.drawRect(x + b * 8, by, 6, bh, kIconColor);
		}
	}
}

void StatusBar::draw(DisplayManager &display, WlanManager &wlan, bool sensorValid, float tempC,
                      float humidityPct, const String &timeHHMM, const String &dateLine,
                      bool showBottomBar, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();

	// --- obere Leiste: Zahnrad, WLAN, DHT11 ---
	tft.fillRect(0, 0, DisplayManager::kScreenWidth, Layout::kStatusBarHeight, bgColor);
	tft.drawFastHLine(0, Layout::kStatusBarHeight - 1, DisplayManager::kScreenWidth, kIconColor);

	drawGearIcon(tft, 18, Layout::kStatusBarHeight / 2, 11);
	drawWifiIcon(tft, 44, Layout::kStatusBarHeight / 2 - 8, wlan.signalBars());

	tft.setTextColor(kIconColor, bgColor);
	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(2);
	String dhtText = sensorValid
	                      ? (String(static_cast<int>(lroundf(tempC))) + "C  " +
	                         String(static_cast<int>(lroundf(humidityPct))) + "%")
	                      : String("--C  --%");
	tft.drawString(dhtText, DisplayManager::kScreenWidth - 8, Layout::kStatusBarHeight / 2);
	tft.setTextDatum(TL_DATUM);

	// --- untere Leiste: Uhrzeit/Datum (entfaellt im Static-Modus mit
	// Datenquelle "Uhrzeit" - die Ansicht nutzt diesen Platz dann selbst) ---
	if (!showBottomBar) {
		return;
	}
	int16_t barY = Layout::kContentBottom;
	tft.fillRect(0, barY, DisplayManager::kScreenWidth, Layout::kStatusBarHeight, bgColor);
	tft.drawFastHLine(0, barY, DisplayManager::kScreenWidth, kIconColor);

	tft.setTextColor(kIconColor, bgColor);
	tft.setTextDatum(ML_DATUM);
	tft.setTextFont(4);
	tft.drawString(timeHHMM.isEmpty() ? "--:--" : timeHHMM, 8, barY + Layout::kStatusBarHeight / 2);

	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(2);
	tft.drawString(dateLine, DisplayManager::kScreenWidth - 8, barY + Layout::kStatusBarHeight / 2);
	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(TFT_BLACK, bgColor);
}
