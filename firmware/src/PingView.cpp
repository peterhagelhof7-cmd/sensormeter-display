#include "PingView.h"

void PingView::drawAverage(DisplayManager &display, const PingManager &ping, int16_t contentTop,
                            int16_t contentBottom, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);

	int16_t midY = contentTop + h / 2;
	if (!ping.hasGoogleReading()) {
		tft.setTextFont(4);
		tft.drawString("Noch keine Ping-Antwort", DisplayManager::kScreenWidth / 2, midY);
	} else {
		char buf[8];
		snprintf(buf, sizeof(buf), "%d", static_cast<int>(lroundf(ping.googleAverageLatencyMs())));
		tft.setTextFont(7);
		tft.drawString(buf, DisplayManager::kScreenWidth / 2 - 10, midY);
		tft.setTextFont(2);
		tft.setTextDatum(ML_DATUM);
		tft.drawString("ms  (google.com)", DisplayManager::kScreenWidth / 2 + 40, midY);
	}
	tft.setTextDatum(TL_DATUM);
}

void PingView::drawTargetList(DisplayManager &display, const PingManager &ping, int16_t contentTop,
                               int16_t contentBottom, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(2);

	size_t count = ping.targetCount();
	if (count == 0) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("Keine Ping-Ziele konfiguriert", DisplayManager::kScreenWidth / 2,
		                contentTop + h / 2);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	int16_t rowH = h / static_cast<int16_t>(count);
	if (rowH > 40) rowH = 40;

	for (size_t i = 0; i < count; i++) {
		int16_t y = contentTop + static_cast<int16_t>(i) * rowH;
		bool checked = ping.targetChecked(i);
		bool ok = ping.targetOk(i);
		uint16_t rowColor = !checked ? TFT_LIGHTGREY : (ok ? TFT_GREEN : TFT_RED);
		tft.fillRect(4, y + 2, DisplayManager::kScreenWidth - 8, rowH - 4, rowColor);
		tft.setTextColor(TFT_BLACK, rowColor);
		tft.setTextDatum(ML_DATUM);
		tft.setTextFont(4);
		String status = !checked ? "..." : (ok ? "OK" : "Fehler");
		tft.drawString(ping.targetIp(i) + "   " + status, 12, y + rowH / 2);
	}
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(TL_DATUM);
}
