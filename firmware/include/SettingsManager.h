#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "DataSource.h"

// Persistiert die Einstellungen aus lastenheft.txt Abschnitt 6 in NVS
// (Namespace "settings"): Betriebsmodus, Slide-Intervall, Static-Quelle,
// Helligkeit. WLAN-Zugangsdaten liegen separat in WlanManager (Namespace
// "wifi").
class SettingsManager {
public:
	static constexpr uint16_t kSlideIntervalMinSec = 5;
	static constexpr uint16_t kSlideIntervalMaxSec = 60;
	static constexpr uint16_t kSlideIntervalStepSec = 5;
	static constexpr uint16_t kSlideIntervalDefaultSec = 10;
	static constexpr uint8_t kBrightnessDefaultPercent = 60;

	void begin();

	OperatingMode mode() const { return mode_; }
	uint16_t slideIntervalSec() const { return slideIntervalSec_; }
	DataSource staticSource() const { return staticSource_; }
	uint8_t brightnessPercent() const { return brightnessPercent_; }

	void setSlideMode(uint16_t intervalSec);
	void setStaticMode(DataSource source);
	void setBrightnessPercent(uint8_t percent);

private:
	void save();

	Preferences prefs;
	OperatingMode mode_ = OperatingMode::Slide;
	uint16_t slideIntervalSec_ = kSlideIntervalDefaultSec;
	DataSource staticSource_ = DataSource::Dht11;
	uint8_t brightnessPercent_ = kBrightnessDefaultPercent;
};
