#include "SettingsManager.h"

void SettingsManager::begin() {
	prefs.begin("settings", true);
	mode_ = static_cast<OperatingMode>(prefs.getUChar("mode", static_cast<uint8_t>(OperatingMode::Slide)));
	slideIntervalSec_ = prefs.getUShort("slideSec", kSlideIntervalDefaultSec);
	staticSource_ = static_cast<DataSource>(prefs.getUChar("staticSrc", static_cast<uint8_t>(DataSource::Dht11)));
	brightnessPercent_ = prefs.getUChar("brightness", kBrightnessDefaultPercent);
	sensormeterIp_ = prefs.getString("smIp", "");
	sensormeterCommunity_ = prefs.getString("smCommunity", "public");
	pingTargetCount_ = prefs.getUChar("pingCount", 0);
	if (pingTargetCount_ > kMaxPingTargets) pingTargetCount_ = kMaxPingTargets;
	for (size_t i = 0; i < pingTargetCount_; i++) {
		char key[8];
		snprintf(key, sizeof(key), "ping%u", static_cast<unsigned>(i));
		pingTargets_[i] = prefs.getString(key, "");
	}
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
	prefs.putString("smIp", sensormeterIp_);
	prefs.putString("smCommunity", sensormeterCommunity_);
	prefs.end();
}

void SettingsManager::savePingTargets() {
	prefs.begin("settings", false);
	prefs.putUChar("pingCount", static_cast<uint8_t>(pingTargetCount_));
	for (size_t i = 0; i < pingTargetCount_; i++) {
		char key[8];
		snprintf(key, sizeof(key), "ping%u", static_cast<unsigned>(i));
		prefs.putString(key, pingTargets_[i]);
	}
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

void SettingsManager::setSensormeterTarget(const String &ip, const String &community) {
	sensormeterIp_ = ip;
	sensormeterCommunity_ = community.isEmpty() ? "public" : community;
	save();
}

bool SettingsManager::addPingTarget(const String &ip) {
	if (pingTargetCount_ >= kMaxPingTargets) {
		return false;
	}
	pingTargets_[pingTargetCount_++] = ip;
	savePingTargets();
	return true;
}

void SettingsManager::removePingTarget(size_t i) {
	if (i >= pingTargetCount_) {
		return;
	}
	for (size_t j = i; j + 1 < pingTargetCount_; j++) {
		pingTargets_[j] = pingTargets_[j + 1];
	}
	pingTargetCount_--;
	savePingTargets();
}
