#include "GraphManager.h"

#include <LittleFS.h>

#include "Layout.h"
#include "TimeSync.h"

namespace {
constexpr const char *kHistoryFile = "/history.csv";
}

void GraphManager::begin() {
	if (!LittleFS.begin(true)) {
		Serial.println("LittleFS-Mount fehlgeschlagen (auch nach Formatierungsversuch)");
		return;
	}
	load();
}

void GraphManager::load() {
	fs::File f = LittleFS.open(kHistoryFile, "r");
	if (!f) {
		return;
	}
	count = 0;
	while (f.available() && count < kMaxEntries) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (line.isEmpty()) {
			continue;
		}
		int firstComma = line.indexOf(',');
		int secondComma = line.indexOf(',', firstComma + 1);
		if (firstComma < 0 || secondComma < 0) {
			continue;
		}
		Entry e;
		e.ts = static_cast<time_t>(line.substring(0, firstComma).toInt());
		e.tempC = static_cast<int16_t>(line.substring(firstComma + 1, secondComma).toInt());
		e.humidityPct = static_cast<int16_t>(line.substring(secondComma + 1).toInt());
		entries[count++] = e;
	}
	f.close();
	if (count > 0) {
		lastRecordTs = entries[count - 1].ts;
	}
}

void GraphManager::save() const {
	fs::File f = LittleFS.open(kHistoryFile, "w");
	if (!f) {
		Serial.println("history.csv konnte nicht geschrieben werden");
		return;
	}
	for (size_t i = 0; i < count; i++) {
		f.printf("%ld,%d,%d\n", static_cast<long>(entries[i].ts), entries[i].tempC,
		         entries[i].humidityPct);
	}
	f.close();
}

void GraphManager::appendEntry(time_t ts, int16_t tempC, int16_t humidityPct) {
	if (count < kMaxEntries) {
		entries[count++] = {ts, tempC, humidityPct};
	} else {
		for (size_t i = 1; i < kMaxEntries; i++) {
			entries[i - 1] = entries[i];
		}
		entries[kMaxEntries - 1] = {ts, tempC, humidityPct};
	}
	lastRecordTs = ts;
	save();
}

void GraphManager::maybeRecord(float tempC, float humidityPct) {
	if (!TimeSync::isValid()) {
		return;
	}
	time_t now = time(nullptr);
	if (lastRecordTs != 0 && (now - lastRecordTs) < static_cast<time_t>(kRecordIntervalSec)) {
		return;
	}
	appendEntry(now, static_cast<int16_t>(lroundf(tempC)), static_cast<int16_t>(lroundf(humidityPct)));
}

void GraphManager::drawGraph(DisplayManager &display, int16_t x, int16_t y, int16_t w, int16_t h) const {
	TFT_eSPI &tft = display.raw();
	tft.drawRect(x, y, w, h, TFT_BLACK);

	if (count < 2) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(2);
		tft.drawString("Noch nicht genug Messwerte", x + w / 2, y + h / 2);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	int16_t tempMin = entries[0].tempC, tempMax = entries[0].tempC;
	int16_t humMin = entries[0].humidityPct, humMax = entries[0].humidityPct;
	for (size_t i = 1; i < count; i++) {
		tempMin = min(tempMin, entries[i].tempC);
		tempMax = max(tempMax, entries[i].tempC);
		humMin = min(humMin, entries[i].humidityPct);
		humMax = max(humMax, entries[i].humidityPct);
	}
	if (tempMax == tempMin) tempMax++;
	if (humMax == humMin) humMax++;

	constexpr int16_t kMargin = 26; // Platz fuer Achsenbeschriftung links/rechts
	int16_t plotX = x + kMargin;
	int16_t plotW = w - 2 * kMargin;
	int16_t plotY = y + 4;
	int16_t plotH = h - 8;

	auto plotPointX = [&](size_t i) -> int16_t {
		if (count == 1) return plotX;
		return plotX + static_cast<int16_t>(static_cast<int32_t>(i) * plotW / (static_cast<int32_t>(count) - 1));
	};
	auto tempPointY = [&](int16_t v) -> int16_t {
		return plotY + plotH - static_cast<int16_t>(static_cast<int32_t>(v - tempMin) * plotH / (tempMax - tempMin));
	};
	auto humPointY = [&](int16_t v) -> int16_t {
		return plotY + plotH - static_cast<int16_t>(static_cast<int32_t>(v - humMin) * plotH / (humMax - humMin));
	};

	for (size_t i = 1; i < count; i++) {
		tft.drawLine(plotPointX(i - 1), tempPointY(entries[i - 1].tempC), plotPointX(i),
		             tempPointY(entries[i].tempC), TFT_RED);
		tft.drawLine(plotPointX(i - 1), humPointY(entries[i - 1].humidityPct), plotPointX(i),
		             humPointY(entries[i].humidityPct), TFT_BLUE);
	}

	tft.setTextFont(1);
	tft.setTextDatum(TR_DATUM);
	tft.setTextColor(TFT_RED, TFT_WHITE);
	tft.drawString(String(tempMax) + "C", x + kMargin - 2, plotY);
	tft.drawString(String(tempMin) + "C", x + kMargin - 2, plotY + plotH - 8);

	tft.setTextDatum(TL_DATUM);
	tft.setTextColor(TFT_BLUE, TFT_WHITE);
	tft.drawString(String(humMax) + "%", x + w - kMargin + 2, plotY);
	tft.drawString(String(humMin) + "%", x + w - kMargin + 2, plotY + plotH - 8);

	tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void GraphManager::drawFullScreen(DisplayManager &display, float currentTempC, float currentHumidityPct,
                                   bool sensorValid) {
	TFT_eSPI &tft = display.raw();
	tft.fillRect(0, Layout::kContentTop, DisplayManager::kScreenWidth, Layout::kContentHeight, TFT_WHITE);

	int16_t valueAreaH = Layout::kContentHeight / 3;
	int16_t valueAreaY = Layout::kContentTop;

	tft.setTextColor(TFT_BLACK, TFT_WHITE);
	if (!sensorValid) {
		tft.setTextDatum(MC_DATUM);
		tft.setTextFont(4);
		tft.drawString("Sensor --", DisplayManager::kScreenWidth / 2, valueAreaY + valueAreaH / 2);
		tft.setTextDatum(TL_DATUM);
	} else {
		char tempBuf[8];
		char humBuf[8];
		snprintf(tempBuf, sizeof(tempBuf), "%d", static_cast<int>(lroundf(currentTempC)));
		snprintf(humBuf, sizeof(humBuf), "%d", static_cast<int>(lroundf(currentHumidityPct)));

		tft.setTextDatum(MR_DATUM);
		tft.setTextFont(7);
		int16_t midY = valueAreaY + valueAreaH / 2;
		tft.drawString(tempBuf, DisplayManager::kScreenWidth / 2 - 6, midY);
		// Eingebaute Fonts kennen kein "°" (nur ASCII 32-127) - stattdessen
		// ein kleiner Kreis von Hand gezeichnet, gefolgt von "C".
		int16_t degX = DisplayManager::kScreenWidth / 2 + 2;
		int16_t degY = midY - 22;
		tft.drawCircle(degX, degY, 3, TFT_BLACK);
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

	int16_t graphY = valueAreaY + valueAreaH;
	int16_t graphH = Layout::kContentHeight - valueAreaH;
	drawGraph(display, 4, graphY, DisplayManager::kScreenWidth - 8, graphH);
}
