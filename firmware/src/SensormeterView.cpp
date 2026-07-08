#include "SensormeterView.h"

void SensormeterView::draw(DisplayManager &display, const SensormeterClient &client, int16_t contentTop,
                            int16_t contentBottom, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);

	int16_t midY = contentTop + h / 2;

	if (!client.isConfigured()) {
		tft.setTextFont(4);
		tft.drawString("Kein Sensormeter-Ziel konfiguriert", DisplayManager::kScreenWidth / 2, midY);
		tft.setTextDatum(TL_DATUM);
		return;
	}
	if (!client.hasValidReading()) {
		tft.setTextFont(4);
		tft.drawString("Sensormeter --", DisplayManager::kScreenWidth / 2, midY);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	char tempBuf[8];
	char humBuf[8];
	snprintf(tempBuf, sizeof(tempBuf), "%.1f", client.temperatureC());
	snprintf(humBuf, sizeof(humBuf), "%.1f", client.humidityPercent());

	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(7);
	tft.drawString(tempBuf, DisplayManager::kScreenWidth / 2 - 6, midY);
	int16_t degX = DisplayManager::kScreenWidth / 2 + 2;
	tft.drawCircle(degX, midY - 22, 3, TFT_BLACK);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("C", degX + 6, midY - 10);

	tft.setTextFont(7);
	tft.setTextDatum(MR_DATUM);
	tft.drawString(humBuf, DisplayManager::kScreenWidth - 30, midY);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("%", DisplayManager::kScreenWidth - 26, midY - 10);

	tft.setTextDatum(TL_DATUM);
}
