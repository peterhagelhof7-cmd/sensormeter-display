#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "DataSource.h"

// Persistiert die Einstellungen aus lastenheft.txt Abschnitt 6 in NVS
// (Namespace "settings"): Betriebsmodus, Slide-Intervall, Static-Quelle,
// Helligkeit, Sensormeter-Ziel, Ping-Ziele. WLAN-Zugangsdaten liegen separat
// in WlanManager (Namespace "wifi").
class SettingsManager {
public:
	static constexpr uint16_t kSlideIntervalMinSec = 5;
	static constexpr uint16_t kSlideIntervalMaxSec = 60;
	static constexpr uint16_t kSlideIntervalStepSec = 5;
	static constexpr uint16_t kSlideIntervalDefaultSec = 10;
	static constexpr uint8_t kBrightnessDefaultPercent = 60;
	static constexpr size_t kMaxPingTargets = 5;

	void begin();

	OperatingMode mode() const { return mode_; }
	uint16_t slideIntervalSec() const { return slideIntervalSec_; }
	DataSource staticSource() const { return staticSource_; }
	uint8_t brightnessPercent() const { return brightnessPercent_; }

	void setSlideMode(uint16_t intervalSec);
	void setStaticMode(DataSource source);
	void setBrightnessPercent(uint8_t percent);

	String sensormeterIp() const { return sensormeterIp_; }
	String sensormeterCommunity() const { return sensormeterCommunity_; }
	void setSensormeterTarget(const String &ip, const String &community);

	size_t pingTargetCount() const { return pingTargetCount_; }
	String pingTargetIp(size_t i) const { return (i < pingTargetCount_) ? pingTargets_[i] : String(); }
	// Liefert false, wenn bereits kMaxPingTargets erreicht sind.
	bool addPingTarget(const String &ip);
	void removePingTarget(size_t i);

private:
	void save();
	void savePingTargets();

	Preferences prefs;
	OperatingMode mode_ = OperatingMode::Slide;
	uint16_t slideIntervalSec_ = kSlideIntervalDefaultSec;
	DataSource staticSource_ = DataSource::Dht11;
	uint8_t brightnessPercent_ = kBrightnessDefaultPercent;

	String sensormeterIp_;
	String sensormeterCommunity_ = "public";

	String pingTargets_[kMaxPingTargets];
	size_t pingTargetCount_ = 0;
};
