#include "SettingsManager.h"

void SettingsManager::begin() {
	mutex_ = xSemaphoreCreateMutex();
	load(); // unlocked: laeuft in begin(), bevor ein zweiter Task existieren kann
}

void SettingsManager::load() {
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
	deviceName_ = prefs.getString("name", "Sensormeter Display");
	webPassword_ = prefs.getString("webPw", "admin");
	prefs.end();

	if (slideIntervalSec_ < kSlideIntervalMinSec || slideIntervalSec_ > kSlideIntervalMaxSec) {
		slideIntervalSec_ = kSlideIntervalDefaultSec;
	}
	if (brightnessPercent_ > 100) {
		brightnessPercent_ = kBrightnessDefaultPercent;
	}
}

// save()/savePingTargets(): nur NVS-Schreiben, KEINE eigene Sperre - werden
// von den Settern aufgerufen, waehrend der Mutex bereits gehalten wird
// (xSemaphoreCreateMutex() ist nicht rekursiv, ein zweites Take vom selben
// Task wuerde blockieren).
void SettingsManager::save() {
	prefs.begin("settings", false);
	prefs.putUChar("mode", static_cast<uint8_t>(mode_));
	prefs.putUShort("slideSec", slideIntervalSec_);
	prefs.putUChar("staticSrc", static_cast<uint8_t>(staticSource_));
	prefs.putUChar("brightness", brightnessPercent_);
	prefs.putString("smIp", sensormeterIp_);
	prefs.putString("smCommunity", sensormeterCommunity_);
	prefs.putString("name", deviceName_);
	prefs.putString("webPw", webPassword_);
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

OperatingMode SettingsManager::mode() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	OperatingMode v = mode_;
	xSemaphoreGive(mutex_);
	return v;
}

uint16_t SettingsManager::slideIntervalSec() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint16_t v = slideIntervalSec_;
	xSemaphoreGive(mutex_);
	return v;
}

DataSource SettingsManager::staticSource() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	DataSource v = staticSource_;
	xSemaphoreGive(mutex_);
	return v;
}

uint8_t SettingsManager::brightnessPercent() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	uint8_t v = brightnessPercent_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setSlideMode(uint16_t intervalSec) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	mode_ = OperatingMode::Slide;
	if (intervalSec < kSlideIntervalMinSec) intervalSec = kSlideIntervalMinSec;
	if (intervalSec > kSlideIntervalMaxSec) intervalSec = kSlideIntervalMaxSec;
	slideIntervalSec_ = intervalSec;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setStaticMode(DataSource source) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	mode_ = OperatingMode::Static;
	staticSource_ = source;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setBrightnessPercent(uint8_t percent) {
	if (percent > 100) percent = 100;
	xSemaphoreTake(mutex_, portMAX_DELAY);
	brightnessPercent_ = percent;
	save();
	xSemaphoreGive(mutex_);
}

String SettingsManager::sensormeterIp() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = sensormeterIp_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::sensormeterCommunity() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = sensormeterCommunity_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setSensormeterTarget(const String &ip, const String &community) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	sensormeterIp_ = ip;
	sensormeterCommunity_ = community.isEmpty() ? "public" : community;
	save();
	xSemaphoreGive(mutex_);
}

size_t SettingsManager::pingTargetCount() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	size_t v = pingTargetCount_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::pingTargetIp(size_t i) const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = (i < pingTargetCount_) ? pingTargets_[i] : String();
	xSemaphoreGive(mutex_);
	return v;
}

bool SettingsManager::addPingTarget(const String &ip) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	bool ok = false;
	if (pingTargetCount_ < kMaxPingTargets) {
		pingTargets_[pingTargetCount_++] = ip;
		savePingTargets();
		ok = true;
	}
	xSemaphoreGive(mutex_);
	return ok;
}

void SettingsManager::removePingTarget(size_t i) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	if (i < pingTargetCount_) {
		for (size_t j = i; j + 1 < pingTargetCount_; j++) {
			pingTargets_[j] = pingTargets_[j + 1];
		}
		pingTargetCount_--;
		savePingTargets();
	}
	xSemaphoreGive(mutex_);
}

String SettingsManager::deviceName() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = deviceName_;
	xSemaphoreGive(mutex_);
	return v;
}

String SettingsManager::webPassword() const {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	String v = webPassword_;
	xSemaphoreGive(mutex_);
	return v;
}

void SettingsManager::setDeviceName(const String &name) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	deviceName_ = name.isEmpty() ? "Sensormeter Display" : name;
	save();
	xSemaphoreGive(mutex_);
}

void SettingsManager::setWebPassword(const String &password) {
	xSemaphoreTake(mutex_, portMAX_DELAY);
	webPassword_ = password.isEmpty() ? "admin" : password;
	save();
	xSemaphoreGive(mutex_);
}
