#include "InfoUI.h"

#include "UiHelpers.h"

namespace {
constexpr int16_t kScreenW = DisplayManager::kScreenWidth;
constexpr int16_t kCloseX = kScreenW - 34;
constexpr int16_t kCloseY = 4;
constexpr int16_t kCloseW = 28;
constexpr int16_t kCloseH = 28;
} // namespace

void InfoUI::run(DisplayManager &display, TouchManager &touch, SettingsManager &settings, WlanManager &wlan) {
	TFT_eSPI &tft = display.raw();
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	tft.setTextDatum(TL_DATUM);
	tft.setTextFont(4);
	tft.drawString("Geraete-Info", 10, 6);
	UiHelpers::drawCloseButton(tft, kCloseX, kCloseY, kCloseW, kCloseH);

	tft.setTextFont(2);
	tft.drawString("Systemname (im Webinterface aenderbar):", 10, 60);
	tft.setTextFont(4);
	tft.drawString(settings.deviceName(), 10, 78);

	tft.setTextFont(2);
	tft.drawString("IP-Adresse:", 10, 130);
	tft.setTextFont(4);
	tft.drawString(WiFi.localIP().toString(), 10, 148);

	tft.setTextFont(2);
	tft.drawString("Verbindungsart:", 10, 190);
	tft.setTextFont(4);
	tft.drawString(wlan.hasStaticIp() ? "Statisch" : "DHCP (automatisch)", 10, 208);

	while (true) {
		int16_t x, y;
		UiHelpers::waitForTapEvent(touch, x, y);
		if (UiHelpers::hitRect(x, y, kCloseX, kCloseY, kCloseW, kCloseH)) {
			return;
		}
	}
}
