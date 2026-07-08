#include "SettingsUI.h"

#include "UiHelpers.h"
#include "DataSource.h"

using UiHelpers::hitRect;
using UiHelpers::waitForTapEvent;

namespace {

constexpr int16_t kScreenW = DisplayManager::kScreenWidth;
constexpr int16_t kScreenH = DisplayManager::kScreenHeight;

constexpr int16_t kCloseX = kScreenW - 34;
constexpr int16_t kCloseY = 4;
constexpr int16_t kCloseW = 28;
constexpr int16_t kCloseH = 28;

constexpr int16_t kMenuRowY = 40;
constexpr int16_t kMenuRowH = 44;

const char *kMenuItems[] = {"Slide", "Static", "Snake", "Systemeinstellungen"};
constexpr size_t kMenuItemCount = 4;

void drawCloseButton(TFT_eSPI &tft) {
	tft.drawRect(kCloseX, kCloseY, kCloseW, kCloseH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("X", kCloseX + kCloseW / 2, kCloseY + kCloseH / 2);
	tft.setTextDatum(TL_DATUM);
}

void drawMenu(DisplayManager &display) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Einstellungen", 10, 6);

	drawCloseButton(tft);

	for (size_t i = 0; i < kMenuItemCount; i++) {
		int16_t y = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		tft.drawRect(10, y, kScreenW - 20, kMenuRowH - 6, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString(kMenuItems[i], kScreenW / 2, y + (kMenuRowH - 6) / 2);
		tft.setTextDatum(TL_DATUM);
	}
}

int hitTestMenu(int16_t x, int16_t y) {
	if (x < 10 || x > kScreenW - 10) return -1;
	for (size_t i = 0; i < kMenuItemCount; i++) {
		int16_t rowY = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		if (y >= rowY && y < rowY + (kMenuRowH - 6)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

constexpr int16_t kMinusX = 60;
constexpr int16_t kPlusX = kScreenW - 60 - 50;
constexpr int16_t kAdjustY = 100;
constexpr int16_t kAdjustW = 50;
constexpr int16_t kAdjustH = 50;
constexpr int16_t kApplyX = kScreenW / 2 - 60;
constexpr int16_t kApplyY = 170;
constexpr int16_t kApplyW = 120;
constexpr int16_t kApplyH = 36;

void drawAdjustScreen(DisplayManager &display, const char *title, const String &valueText) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString(title, 10, 6);
	drawCloseButton(tft);

	tft.drawRect(kMinusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.drawRect(kPlusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("-", kMinusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);
	tft.drawString("+", kPlusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);

	tft.fillRect(kMinusX + kAdjustW + 4, kAdjustY, kPlusX - (kMinusX + kAdjustW + 4), kAdjustH, TFT_WHITE);
	tft.setTextFont(7);
	tft.drawString(valueText, kScreenW / 2, kAdjustY + kAdjustH / 2);

	tft.drawRect(kApplyX, kApplyY, kApplyW, kApplyH, TFT_BLACK);
	tft.setTextFont(2);
	tft.drawString("Uebernehmen", kApplyX + kApplyW / 2, kApplyY + kApplyH / 2);
	tft.setTextDatum(TL_DATUM);
}

// Slide-Intervall waehlen (5..60s in 5s-Schritten). Rueckgabe true, wenn
// uebernommen (Aufrufer soll SettingsUI dann komplett verlassen).
bool runSlideConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	uint16_t interval = settings.slideIntervalSec();

	auto redraw = [&]() { drawAdjustScreen(display, "Slide-Intervall", String(interval) + "s"); };
	redraw();

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return false;
		}
		if (hitRect(x, y, kMinusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (interval > SettingsManager::kSlideIntervalMinSec) interval -= SettingsManager::kSlideIntervalStepSec;
			redraw();
		} else if (hitRect(x, y, kPlusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (interval < SettingsManager::kSlideIntervalMaxSec) interval += SettingsManager::kSlideIntervalStepSec;
			redraw();
		} else if (hitRect(x, y, kApplyX, kApplyY, kApplyW, kApplyH)) {
			settings.setSlideMode(interval);
			return true;
		}
	}
}

void drawStaticSourceList(DisplayManager &display) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Static-Datenquelle", 10, 6);
	drawCloseButton(tft);

	for (size_t i = 0; i < kAvailableDataSourceCount; i++) {
		int16_t y = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		tft.drawRect(10, y, kScreenW - 20, kMenuRowH - 6, TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString(dataSourceLabel(kAvailableDataSources[i]), kScreenW / 2, y + (kMenuRowH - 6) / 2);
		tft.setTextDatum(TL_DATUM);
	}
}

int hitTestSourceList(int16_t x, int16_t y) {
	if (x < 10 || x > kScreenW - 10) return -1;
	for (size_t i = 0; i < kAvailableDataSourceCount; i++) {
		int16_t rowY = kMenuRowY + static_cast<int16_t>(i) * kMenuRowH;
		if (y >= rowY && y < rowY + (kMenuRowH - 6)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool runStaticConfig(DisplayManager &display, TouchManager &touch, SettingsManager &settings) {
	drawStaticSourceList(display);
	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return false;
		}
		int idx = hitTestSourceList(x, y);
		if (idx >= 0) {
			settings.setStaticMode(kAvailableDataSources[static_cast<size_t>(idx)]);
			return true;
		}
	}
}

void runSnakePlaceholder(DisplayManager &display, TouchManager &touch) {
	display.drawBootScreen("Snake", "Noch nicht implementiert (folgt in P6)");
	int16_t x, y;
	waitForTapEvent(touch, x, y);
}

constexpr int16_t kWlanButtonX = kScreenW / 2 - 90;
constexpr int16_t kWlanButtonY = 170;
constexpr int16_t kWlanButtonW = 180;
constexpr int16_t kWlanButtonH = 36;

void drawSystemSettings(DisplayManager &display, uint8_t brightnessPercent) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Systemeinstellungen", 10, 6);
	drawCloseButton(tft);

	tft.setTextFont(2);
	tft.drawString("Helligkeit", 60, kAdjustY - 20);

	tft.drawRect(kMinusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.drawRect(kPlusX, kAdjustY, kAdjustW, kAdjustH, TFT_BLACK);
	tft.setTextDatum(MC_DATUM);
	tft.setTextFont(4);
	tft.drawString("-", kMinusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);
	tft.drawString("+", kPlusX + kAdjustW / 2, kAdjustY + kAdjustH / 2);

	tft.fillRect(kMinusX + kAdjustW + 4, kAdjustY, kPlusX - (kMinusX + kAdjustW + 4), kAdjustH, TFT_WHITE);
	tft.setTextFont(7);
	tft.drawString(String(brightnessPercent) + "%", kScreenW / 2, kAdjustY + kAdjustH / 2);

	tft.drawRect(kWlanButtonX, kWlanButtonY, kWlanButtonW, kWlanButtonH, TFT_BLACK);
	tft.setTextFont(2);
	tft.drawString("WLAN neu waehlen", kWlanButtonX + kWlanButtonW / 2, kWlanButtonY + kWlanButtonH / 2);
	tft.setTextDatum(TL_DATUM);
}

// Helligkeit +/- und WLAN-Neuauswahl. Kehrt zum Aufrufer (Hauptmenu) zurueck,
// aendert also nie den Betriebsmodus - daher kein bool-Rueckgabewert noetig.
void runSystemSettings(DisplayManager &display, TouchManager &touch, WlanManager &wlan,
                       SettingsManager &settings, BacklightManager &backlight,
                       WifiOnboarding &onboarding) {
	uint8_t brightness = settings.brightnessPercent();
	drawSystemSettings(display, brightness);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}
		if (hitRect(x, y, kMinusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (brightness >= 5) brightness -= 5;
			backlight.setPercent(brightness);
			settings.setBrightnessPercent(brightness);
			drawSystemSettings(display, brightness);
		} else if (hitRect(x, y, kPlusX, kAdjustY, kAdjustW, kAdjustH)) {
			if (brightness <= 95) brightness += 5;
			backlight.setPercent(brightness);
			settings.setBrightnessPercent(brightness);
			drawSystemSettings(display, brightness);
		} else if (hitRect(x, y, kWlanButtonX, kWlanButtonY, kWlanButtonW, kWlanButtonH)) {
			onboarding.run(display, touch, wlan);
			drawSystemSettings(display, brightness);
		}
	}
}

} // namespace

void SettingsUI::run(DisplayManager &display, TouchManager &touch, WlanManager &wlan,
                      SettingsManager &settings, BacklightManager &backlight,
                      WifiOnboarding &onboarding) {
	drawMenu(display);

	while (true) {
		int16_t x, y;
		waitForTapEvent(touch, x, y);

		if (hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}

		int idx = hitTestMenu(x, y);
		if (idx < 0) {
			continue;
		}

		bool exitSettings = false;
		switch (idx) {
			case 0: // Slide
				exitSettings = runSlideConfig(display, touch, settings);
				break;
			case 1: // Static
				exitSettings = runStaticConfig(display, touch, settings);
				break;
			case 2: // Snake
				runSnakePlaceholder(display, touch);
				break;
			case 3: // Systemeinstellungen
				runSystemSettings(display, touch, wlan, settings, backlight, onboarding);
				break;
		}

		if (exitSettings) {
			return;
		}
		drawMenu(display);
	}
}
