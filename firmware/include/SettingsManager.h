#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>

#include "DataSource.h"

// Persistiert die Einstellungen aus lastenheft.txt Abschnitt 6 in NVS
// (Namespace "settings"): Betriebsmodus, Slide-Intervall, Static-Quelle,
// Helligkeit, Sensormeter-Ziel, Ping-Ziele, Geraetename + Web-Passwort
// (siehe lastenheft.txt Abschnitt 8, Webserver-Zusatzfunktion). WLAN-
// Zugangsdaten liegen separat in WlanManager (Namespace "wifi").
//
// Mutex-geschuetzt (wie DataManager im Sensormeter-Projekt): wird sowohl
// aus dem Hauptloop/Touch-UI als auch aus den asynchronen Webserver-
// Callbacks (ESPAsyncWebServer laeuft in einem eigenen FreeRTOS-Task,
// nicht im Arduino-Hauptloop) gleichzeitig gelesen/geschrieben - ohne
// Sperre waere das ein Datenrennen auf den String-Feldern.
class SettingsManager {
public:
	static constexpr uint16_t kSlideIntervalMinSec = 5;
	static constexpr uint16_t kSlideIntervalMaxSec = 60;
	static constexpr uint16_t kSlideIntervalStepSec = 5;
	static constexpr uint16_t kSlideIntervalDefaultSec = 10;
	static constexpr uint8_t kBrightnessDefaultPercent = 60;
	static constexpr size_t kMaxPingTargets = 5;
	static constexpr size_t kMaxSensormeterTargets = 5;

	void begin();

	OperatingMode mode() const;
	uint16_t slideIntervalSec() const;
	DataSource staticSource() const;
	uint8_t brightnessPercent() const;

	void setSlideMode(uint16_t intervalSec);
	void setStaticMode(DataSource source);
	void setBrightnessPercent(uint8_t percent);

	// Bis zu kMaxSensormeterTargets Ziele, eine gemeinsame SNMP-Community fuer
	// alle (siehe docs/entscheidungen.md). Wie bei den Ping-Zielen ist 0 der
	// technische Startzustand eines frisch geflashten Geraets; die
	// Bedienoberflaechen (Touch/Web) verhindern aber das Entfernen des
	// letzten verbliebenen Ziels, sodass im laufenden Betrieb immer
	// mindestens eins konfiguriert bleibt, sobald eins hinzugefuegt wurde.
	String sensormeterCommunity() const;
	void setSensormeterCommunity(const String &community);

	size_t sensormeterTargetCount() const;
	String sensormeterTargetIp(size_t i) const;
	// Liefert false, wenn bereits kMaxSensormeterTargets erreicht sind.
	bool addSensormeterTarget(const String &ip);
	void removeSensormeterTarget(size_t i);

	size_t pingTargetCount() const;
	String pingTargetIp(size_t i) const;
	// Liefert false, wenn bereits kMaxPingTargets erreicht sind.
	bool addPingTarget(const String &ip);
	void removePingTarget(size_t i);

	String deviceName() const;
	String webPassword() const;
	void setDeviceName(const String &name);
	void setWebPassword(const String &password);

private:
	void load();
	void save();
	void savePingTargets();
	void saveSensormeterTargets();

	Preferences prefs;
	// Nicht mutable: Take/Give aendern nur den internen FreeRTOS-Zustand des
	// Semaphors, nicht den Wert des Handles selbst - const-Methoden duerfen
	// das Handle unveraendert weiterreichen.
	SemaphoreHandle_t mutex_ = nullptr;

	OperatingMode mode_ = OperatingMode::Slide;
	uint16_t slideIntervalSec_ = kSlideIntervalDefaultSec;
	DataSource staticSource_ = DataSource::Dht11;
	uint8_t brightnessPercent_ = kBrightnessDefaultPercent;

	String sensormeterTargets_[kMaxSensormeterTargets];
	size_t sensormeterTargetCount_ = 0;
	String sensormeterCommunity_ = "public";

	String pingTargets_[kMaxPingTargets];
	size_t pingTargetCount_ = 0;

	String deviceName_ = "Sensormeter Display";
	String webPassword_ = "admin";
};
