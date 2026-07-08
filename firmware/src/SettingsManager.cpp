#include "SettingsManager.h"

void SettingsManager::begin() {
	prefs.begin("settings", true);
	mode_ = static_cast<OperatingMode>(prefs.getUChar("mode", static_cast<uint8_t>(OperatingMode::Slide)));
	slideIntervalSec_ = prefs.getUShort("slideSec", kSlideIntervalDefaultSec);
	staticSource_ = static_cast<DataSource>(prefs.getUChar("staticSrc", static_cast<uint8_t>(DataSource::Dht11)));
	brightnessPercent_ = prefs.getUChar("brightness", kBrightnessDefaultPercent);
	prefs.end();

	if (slideIntervalSec_ < kSlideIntervalMinSec || slideIntervalSec_ > kSlideIntervalMaxSec) {
		slideIntervalSec_ = kSlideIntervalDefaultSec;
	}
	if (brightnessPercent_ > 100) {
		brightnessPercent_ = kBrightnessDefaultPercent;
	}
}

void SettingsManager::save() {
	prefs.begin("settings", false);
	prefs.putUChar("mode", static_cast<uint8_t>(mode_));
	prefs.putUShort("slideSec", slideIntervalSec_);
	prefs.putUChar("staticSrc", static_cast<uint8_t>(staticSource_));
	prefs.putUChar("brightness", brightnessPercent_);
	prefs.end();
}

void SettingsManager::setSlideMode(uint16_t intervalSec) {
	mode_ = OperatingMode::Slide;
	if (intervalSec < kSlideIntervalMinSec) intervalSec = kSlideIntervalMinSec;
	if (intervalSec > kSlideIntervalMaxSec) intervalSec = kSlideIntervalMaxSec;
	slideIntervalSec_ = intervalSec;
	save();
}

void SettingsManager::setStaticMode(DataSource source) {
	mode_ = OperatingMode::Static;
	staticSource_ = source;
	save();
}

void SettingsManager::setBrightnessPercent(uint8_t percent) {
	if (percent > 100) percent = 100;
	brightnessPercent_ = percent;
	save();
}
