#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

// Sehr schlanker SNMP-v1-GET-Client fuer genau einen skalaren Integer-Wert
// pro Aufruf. Kein allgemeiner SNMP-Stack: die Sensormeter-Gegenstelle hat
// eine feste, bekannte OID-Struktur (siehe lastenheft.txt Abschnitt 7.3),
// eine volle SNMP-Bibliothek waere unverhaeltnismaessiger Aufwand fuer zwei
// Werte alle 30 Sekunden. BER-Kodierung/-Dekodierung nur im Umfang, den
// diese feste Paketform braucht (kurze Laengen, keine Geschwister-Elemente
// auf oberster Ebene) - siehe docs/entscheidungen.md.
class SnmpClient {
public:
	// Liefert true und schreibt outValue, wenn innerhalb von timeoutMs eine
	// Antwort mit einem INTEGER-Wert eintraf.
	bool getInteger(const String &host, const String &community, const String &oidDotted,
	                 int32_t &outValue, uint16_t port = 161, uint32_t timeoutMs = 2000);

private:
	WiFiUDP udp;
};
