#include "SensormeterView.h"

namespace {
struct SlideRef {
	size_t targetIndex;
	uint8_t sensorIndex;
};
} // namespace

void SensormeterView::draw(DisplayManager &display, const SensormeterManager &manager, int16_t contentTop,
                            int16_t contentBottom, uint16_t bgColor) {
	TFT_eSPI &tft = display.raw();
	int16_t h = contentBottom - contentTop;
	tft.fillRect(0, contentTop, DisplayManager::kScreenWidth, h, bgColor);
	tft.setTextColor(TFT_BLACK, bgColor);
	tft.setTextDatum(MC_DATUM);

	int16_t midY = contentTop + h / 2;

	if (manager.targetCount() == 0) {
		tft.setTextFont(4);
		tft.drawString("Kein Sensormeter-Ziel konfiguriert", DisplayManager::kScreenWidth / 2, midY);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	SlideRef slides[SensormeterManager::kMaxTargets * 2];
	size_t slideCount = 0;
	for (size_t i = 0; i < manager.targetCount(); i++) {
		if (!manager.isResolved(i)) continue;
		slides[slideCount++] = {i, 0};
		if (manager.isPro(i)) {
			slides[slideCount++] = {i, 1};
		}
	}

	if (slideCount == 0) {
		tft.setTextFont(4);
		tft.drawString("Sensormeter --", DisplayManager::kScreenWidth / 2, midY);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	uint32_t now = millis();
	if (now - lastRotateMs_ >= kSubSlideIntervalMs) {
		lastRotateMs_ = now;
		currentSlide_ = (currentSlide_ + 1) % slideCount;
	}
	if (currentSlide_ >= slideCount) {
		currentSlide_ = 0;
	}
	const SlideRef &slide = slides[currentSlide_];

	String title = manager.systemName(slide.targetIndex);
	String sensorName = manager.sensorName(slide.targetIndex, slide.sensorIndex);
	if (!sensorName.isEmpty()) {
		title += " (" + sensorName + ")";
	}
	tft.setTextFont(2);
	tft.drawString(title, DisplayManager::kScreenWidth / 2, contentTop + 14);

	if (!manager.sensorValid(slide.targetIndex, slide.sensorIndex)) {
		tft.setTextFont(4);
		tft.drawString("--", DisplayManager::kScreenWidth / 2, midY + 6);
		tft.setTextDatum(TL_DATUM);
		return;
	}

	char tempBuf[8];
	char humBuf[8];
	snprintf(tempBuf, sizeof(tempBuf), "%.1f", manager.sensorTempC(slide.targetIndex, slide.sensorIndex));
	snprintf(humBuf, sizeof(humBuf), "%.1f", manager.sensorHumidityPct(slide.targetIndex, slide.sensorIndex));

	tft.setTextDatum(MR_DATUM);
	tft.setTextFont(7);
	tft.drawString(tempBuf, DisplayManager::kScreenWidth / 2 - 6, midY + 10);
	int16_t degX = DisplayManager::kScreenWidth / 2 + 2;
	tft.drawCircle(degX, midY - 12, 3, TFT_BLACK);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("C", degX + 6, midY);

	tft.setTextFont(7);
	tft.setTextDatum(MR_DATUM);
	tft.drawString(humBuf, DisplayManager::kScreenWidth - 30, midY + 10);
	tft.setTextFont(2);
	tft.setTextDatum(ML_DATUM);
	tft.drawString("%", DisplayManager::kScreenWidth - 26, midY);

	tft.setTextDatum(TL_DATUM);
}
