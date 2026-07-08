#include "DisplayManager.h"

void DisplayManager::begin() {
	tft.init();
	// TFT_eSPI schaltet fuer ST7789_DRIVER standardmaessig INVON (Farb-
	// invertierung) ein. Auf diesem Panel fuehrte das dazu, dass Weiss als
	// Schwarz und alle Buntfarben verfaelscht erschienen (z.B. Rot als Gelb) -
	// hier explizit wieder ausgeschaltet. Siehe docs/entscheidungen.md fuer
	// die Fehlersuche (RGB/BGR-Reihenfolge war eine Sackgasse, nicht die
	// Ursache).
	tft.invertDisplay(false);
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
