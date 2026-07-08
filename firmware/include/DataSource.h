#pragma once

#include <cstdint>

// Datenquellen, die aktuell implementiert sind. Sensormeter (P7) und Ping
// (P8) werden ergaenzt, sobald sie existieren - siehe docs/entscheidungen.md.
enum class DataSource : uint8_t {
	Dht11 = 0,
	Uhrzeit = 1,
};

enum class OperatingMode : uint8_t {
	Slide = 0,
	Static = 1,
};

constexpr DataSource kAvailableDataSources[] = {DataSource::Dht11, DataSource::Uhrzeit};
constexpr size_t kAvailableDataSourceCount = 2;

inline const char *dataSourceLabel(DataSource s) {
	switch (s) {
		case DataSource::Dht11: return "Innentemperatur (DHT11)";
		case DataSource::Uhrzeit: return "Uhrzeit";
	}
	return "?";
}
