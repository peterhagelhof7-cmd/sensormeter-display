#pragma once

#include <cstdint>

// Alle implementierten Datenquellen (P2-P8).
enum class DataSource : uint8_t {
	Dht11 = 0,
	Uhrzeit = 1,
	Sensormeter = 2,
	Ping = 3,
	PingTargets = 4,
	// Anbieter-Branding (Weisslabel) - wie die anderen Quellen immer Teil
	// der Rotation/Auswahl, auch unkonfiguriert (zeigt dann einen
	// Platzhalter, siehe BrandingView) - bewusst KEIN dynamisches
	// Ein-/Ausblenden wie bei den OLED-Geschwisterprojekten, da diese
	// Liste hier ohnehin nie nach Konfigurationsstand filtert (Sensormeter/
	// Ping-Ziele bleiben ebenso in der Rotation, wenn nichts konfiguriert
	// ist).
	Branding = 5,
};

enum class OperatingMode : uint8_t {
	Slide = 0,
	Static = 1,
};

constexpr DataSource kAvailableDataSources[] = {
    DataSource::Dht11, DataSource::Uhrzeit, DataSource::Sensormeter, DataSource::Ping,
    DataSource::PingTargets, DataSource::Branding,
};
constexpr size_t kAvailableDataSourceCount = 6;

// Kurz gehalten, da diese Labels in einer schmalen Listenzeile (Font 4,
// ca. 300px Breite) Platz finden muessen - siehe SettingsUI.cpp.
inline const char *dataSourceLabel(DataSource s) {
	switch (s) {
		case DataSource::Dht11: return "DHT11 (intern)";
		case DataSource::Uhrzeit: return "Uhrzeit";
		case DataSource::Sensormeter: return "Sensormeter";
		case DataSource::Ping: return "Ping";
		case DataSource::PingTargets: return "Ping-Ziele";
		case DataSource::Branding: return "Branding";
	}
	return "?";
}
