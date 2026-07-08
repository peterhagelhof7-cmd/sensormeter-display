#include "DisplayManager.h"

void DisplayManager::begin() {
	tft.init();
	tft.setRotation(1); // Querbetrieb, 320x240
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void DisplayManager::drawBootScreen(const char *line1, const char *line2) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextDatum(MC_DATUM);
	tft.setFreeFont(nullptr);
	tft.setTextFont(4);
	tft.drawString(line1, kScreenWidth / 2, kScreenHeight / 2 - 14);
	tft.setTextFont(2);
	tft.drawString(line2, kScreenWidth / 2, kScreenHeight / 2 + 14);
}
