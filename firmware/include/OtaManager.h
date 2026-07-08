#pragma once

#include <Arduino.h>

// Lokales OTA-Update per .bin-Upload ueber den Webserver (kein
// GitHub-Versionscheck/-Direktinstall - ein HTTPS-Client (WiFiClientSecure/
// HTTPClient) haette allein ca. 168 KB Flash gekostet, siehe
// docs/entscheidungen.md und das analoge Vorgehen im Sensormeter-Projekt).
// Braucht die Zwei-Slot-Partitionstabelle (partitions_ota.csv).
class OtaManager {
public:
	bool beginLocalUpdate(size_t contentLength);
	bool writeLocalUpdateChunk(uint8_t *data, size_t len);
	bool endLocalUpdate();
};
