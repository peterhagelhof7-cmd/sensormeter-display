#include <Arduino.h>
#include <ESPmDNS.h>
#include <math.h>

#include "DisplayManager.h"
#include "TouchManager.h"
#include "WlanManager.h"
#include "WifiOnboarding.h"
#include "SensorManager.h"
#include "GraphManager.h"
#include "ClockView.h"
#include "SensormeterManager.h"
#include "SensormeterView.h"
#include "PingManager.h"
#include "PingView.h"
#include "LedManager.h"
#include "StatusBar.h"
#include "TimeSync.h"
#include "Layout.h"
#include "DataSource.h"
#include "SettingsManager.h"
#include "BacklightManager.h"
#include "SettingsUI.h"
#include "UiHelpers.h"
#include "OtaManager.h"
#include "WebServerManager.h"
#include "InfoUI.h"
#include "AlertEvaluator.h"
#include "BrandingManager.h"
#include "BrandingView.h"

namespace {
// Wandelt den frei waehlbaren Systemnamen in einen gueltigen mDNS-Hostnamen
// um (nur a-z/0-9/-, keine Umlaute/Leerzeichen) - Systemname kann Leer-
// zeichen/Umlaute/Grossbuchstaben enthalten, mDNS-Hostnamen duerfen das
// nicht. Fallback auf einen festen Namen, falls nach dem Filtern nichts
// uebrig bleibt (z.B. Systemname nur aus Umlauten).
String sanitizeHostname(const String &name) {
	String out;
	for (size_t i = 0; i < name.length(); i++) {
		char c = name[i];
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
			out += c;
		} else if ((c == ' ' || c == '_') && out.length() > 0 && out[out.length() - 1] != '-') {
			out += '-';
		}
	}
	while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
	while (out.length() > 0 && out[0] == '-') out.remove(0, 1);
	if (out.isEmpty()) out = "sensormeter-display";
	return out;
}
} // namespace

DisplayManager display;
TouchManager touch;
WlanManager wlan;
WifiOnboarding onboarding;
SensorManager sensor;
GraphManager graph;
ClockView clockView;
SensormeterManager sensormeterManager;
SensormeterView sensormeterView;
PingManager pingManager;
PingView pingView;
LedManager led;
StatusBar statusBar;
SettingsManager settings;
BacklightManager backlight;
SettingsUI settingsUI;
OtaManager ota;
BrandingManager brandingManager;
BrandingView brandingView;
WebServerManager webServer(settings, backlight, ota, wlan, sensormeterManager, sensor, pingManager, graph,
                            brandingManager);

// Serial-Kommandozeile fuer den Fall, dass das Geraet nur per USB, aber
// nicht per Netzwerk erreichbar ist (z.B. falsche/unbekannte
// WLAN-Zugangsdaten, statische IP in einem fremden Netzsegment). Bewusst
// dasselbe Vertrauensmodell wie bei den drei OLED-Geschwisterprojekten
// (physischer USB-Zugriff = vertrauenswuerdig, kein Web-Passwort noetig) -
// NICHT pauschal destruktiv: nur "reset"/"reset all" loeschen etwas, alle
// anderen Kommandos aendern gezielt nur die WLAN-/IP-Felder. Anders als bei
// den Geschwisterprojekten gibt es hier kein "dump"/"upload" (kein
// XML-Konfigurationsdokument - Einstellungen liegen einzeln in NVS/
// Preferences, siehe SettingsManager/WlanManager), siehe docs/entscheidungen.md.
//
// Kommandos (jeweils + Enter):
//   dhcp                          WLAN auf DHCP umstellen, statische
//                                  IP/Maske/Gateway loeschen, neu starten
//   ip <ip> <maske> <gateway> [dns]  statische IP setzen, neu starten. Anders
//                                  als die Einstellungsseite OHNE
//                                  Ping-Kollisionspruefung - bewusst einfach
//                                  gehalten. DNS optional (4. Argument); fehlt
//                                  er, nutzt WlanManager das Gateway als DNS.
//   wifi <ssid> <passwort>        neue WLAN-Zugangsdaten setzen, neu starten
//   status                        aktuellen Zustand ausgeben (WLAN, IP,
//                                  Signal, Sensor, Heap, Laufzeit) - liest
//                                  nur, aendert nichts, kein Neustart
//   reset                         Werksreset nur der Konfiguration (wie
//                                  Umfang "Nur Konfiguration" auf der
//                                  Einstellungsseite, inkl. WLAN, ohne
//                                  Branding-Erhalt), neu starten - der
//                                  12h-Verlauf bleibt erhalten
//   reset all                     wie oben, zusaetzlich wird der 12h-Verlauf
//                                  geloescht UND das Branding entfernt
void handleSerialCommands() {
	static String line;

	while (Serial.available()) {
		char c = static_cast<char>(Serial.read());
		if (c == '\r') continue;
		if (c != '\n') {
			line += c;
			continue;
		}
		line.trim();

		String cmd = line;
		String args;
		int sp = line.indexOf(' ');
		if (sp >= 0) {
			cmd = line.substring(0, sp);
			args = line.substring(sp + 1);
			args.trim();
		}

		if (cmd.equalsIgnoreCase("dhcp")) {
			wlan.clearStaticIp();
			Serial.println("[SERIAL] WLAN auf DHCP umgestellt, starte neu...");
			delay(300);
			ESP.restart();

		} else if (cmd.equalsIgnoreCase("ip")) {
			String parts[4];
			int count = 0;
			String rest = args;
			while (rest.length() > 0 && count < 4) {
				int sp2 = rest.indexOf(' ');
				if (sp2 < 0) {
					parts[count++] = rest;
					rest = "";
				} else {
					parts[count++] = rest.substring(0, sp2);
					rest = rest.substring(sp2 + 1);
					rest.trim();
				}
			}
			IPAddress ip, mask, gateway, dns;
			if (count < 3 || !ip.fromString(parts[0]) || !mask.fromString(parts[1]) ||
			    !gateway.fromString(parts[2])) {
				Serial.println("[SERIAL] Nutzung: ip <adresse> <maske> <gateway> [dns]");
			} else {
				// DNS optional (4. Argument): fehlt/ungueltig -> 0.0.0.0, dann
				// verwendet WlanManager das Gateway als DNS.
				if (count < 4 || !dns.fromString(parts[3])) {
					dns = IPAddress((uint32_t)0);
				}
				wlan.saveStaticIp(ip, gateway, mask, dns);
				Serial.println("[SERIAL] Statische IP gesetzt, starte neu...");
				delay(300);
				ESP.restart();
			}

		} else if (cmd.equalsIgnoreCase("wifi")) {
			int sp3 = args.indexOf(' ');
			if (sp3 < 0 || args.substring(0, sp3).length() == 0) {
				Serial.println("[SERIAL] Nutzung: wifi <ssid> <passwort>");
			} else {
				String ssid = args.substring(0, sp3);
				String psk = args.substring(sp3 + 1);
				psk.trim();
				wlan.saveCredentials(ssid, psk);
				Serial.println("[SERIAL] WLAN-Zugangsdaten gesetzt, starte neu...");
				delay(300);
				ESP.restart();
			}

		} else if (cmd.equalsIgnoreCase("status")) {
			Serial.println("[SERIAL] --- Status ---");
			Serial.print("WLAN: ");
			if (wlan.isConnected()) {
				Serial.println("verbunden");
			} else {
				Serial.println("nicht verbunden");
			}
			Serial.print("IP: ");
			Serial.println(WiFi.localIP());
			Serial.print("Signal: ");
			Serial.print(wlan.isConnected() ? WiFi.RSSI() : 0);
			Serial.println(" dBm");
			Serial.print("Sensor: ");
			if (sensor.hasValidReading()) {
				Serial.print(sensor.temperatureC(), 1);
				Serial.print(" C / ");
				Serial.print(sensor.humidityPercent(), 1);
				Serial.println(" %");
			} else {
				Serial.println("kein gueltiger Messwert");
			}
			Serial.print("Freier Heap: ");
			Serial.print(ESP.getFreeHeap() / 1024);
			Serial.println(" kB");
			Serial.print("Laufzeit: ");
			Serial.print((unsigned long)(esp_timer_get_time() / 1000000ULL));
			Serial.println(" s");
			Serial.println("[SERIAL] --- Ende Status ---");

		} else if (cmd.equalsIgnoreCase("reset")) {
			bool full = args.equalsIgnoreCase("all");
			settings.resetConfig(/*keepBrandingName=*/!full);
			wlan.clearCredentials();
			if (full) {
				graph.reset();
				brandingManager.deleteLogo();
				Serial.println("[SERIAL] Werksreset: Konfiguration, Verlauf und Branding geloescht, starte neu...");
			} else {
				Serial.println("[SERIAL] Werksreset: Konfiguration auf Standard zurueckgesetzt, starte neu...");
			}
			delay(300);
			ESP.restart();
		}

		line = "";
	}
}

uint32_t lastStatusBarMs = 0;
// 300ms statt z.B. 1000ms, damit das 500ms-Blinken des WLAN-Symbols nicht
// mit der Redraw-Rate aliast (bei exaktem 1000ms-Takt wuerde immer dieselbe
// Blink-Phase abgetastet und das Symbol nie sichtbar blinken).
constexpr uint32_t kStatusBarIntervalMs = 300;

uint32_t slideLastSwitchMs = 0;
size_t slideIndex = 0;
bool contentDirty = true;
uint32_t lastPeriodicRedrawMs = 0;
constexpr uint32_t kPeriodicRedrawIntervalMs = 5000;

void setup() {
	Serial.begin(115200);
	delay(200);
	Serial.println("Sensormeter Display - Boot");
	Serial.println("[SERIAL] Kommandos: dhcp, ip, wifi, status, reset[ all] (+ Enter)");

	display.begin();
	display.drawBootScreen("Sensormeter Display", "P7/P8: Sensormeter + Ping");

	touch.begin();
	if (!touch.isCalibrated()) {
		Serial.println("Keine Touch-Kalibrierung gefunden - starte Kalibrierroutine");
		touch.runCalibration(display);
	}

	settings.begin();
	backlight.begin(settings.brightnessPercent());
	led.begin();

	wlan.begin();
	bool connected = false;
	if (wlan.hasCredentials()) {
		display.drawBootScreen("WLAN", "Verbinde mit gespeichertem Netz ...");
		connected = wlan.autoConnect();
	}
	if (!connected) {
		Serial.println("Kein WLAN verbunden - starte Onboarding");
		onboarding.run(display, touch, wlan);
	}
	Serial.print("WLAN verbunden, IP: ");
	Serial.println(WiFi.localIP());

	// mDNS: Geraet unter http://<hostname>.local/ erreichbar, ohne die IP
	// erst am Geraet ablesen zu muessen. Hostname wird bei jedem Boot neu
	// aus dem aktuellen Systemnamen abgeleitet - eine spaetere Aenderung des
	// Systemnamens wirkt sich wie bei den Netzwerkeinstellungen erst nach
	// einem Neustart aus.
	String mdnsHost = sanitizeHostname(settings.deviceName());
	if (MDNS.begin(mdnsHost.c_str())) {
		MDNS.addService("http", "tcp", 80);
		Serial.print("mDNS gestartet: http://");
		Serial.print(mdnsHost);
		Serial.println(".local/");
	} else {
		Serial.println("mDNS konnte nicht gestartet werden");
	}

	TimeSync::begin();
	sensor.begin();
	graph.begin();
	pingManager.begin();
	sensormeterManager.begin();
	brandingManager.begin();
	webServer.begin();
}

DataSource currentActiveSource() {
	if (settings.mode() == OperatingMode::Static) {
		return settings.staticSource();
	}

	uint32_t now = millis();
	uint32_t intervalMs = static_cast<uint32_t>(settings.slideIntervalSec()) * 1000UL;
	if (now - slideLastSwitchMs >= intervalMs) {
		slideLastSwitchMs = now;
		slideIndex = (slideIndex + 1) % kAvailableDataSourceCount;
		contentDirty = true;
	}
	return kAvailableDataSources[slideIndex];
}

void loop() {
	handleSerialCommands();

	// Zahnrad in der Statusleiste antippbar - oeffnet die Einstellungen
	// (blockierend). Erst auf Loslassen warten, damit der modale Dialog
	// nicht denselben, noch gehaltenen Tipp als seinen ersten Tastendruck
	// missversteht.
	int16_t tx, ty;
	bool touchedNow = touch.read(tx, ty);
	if (touchedNow &&
	    UiHelpers::hitRect(tx, ty, StatusBar::kGearHitX, StatusBar::kGearHitY, StatusBar::kGearHitW,
	                        StatusBar::kGearHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		settingsUI.run(display, touch, wlan, settings, backlight, onboarding);
		contentDirty = true;
		lastStatusBarMs = 0;
		graph.forceRedraw();
		pingView.forceRedraw();
		statusBar.forceRedraw();
		brandingView.forceRedraw();
	}

	// Info-Symbol antippbar - oeffnet InfoUI (Systemname/IP/DHCP-Static),
	// gleiches Muster wie das Zahnrad oben.
	if (touchedNow && UiHelpers::hitRect(tx, ty, StatusBar::kInfoHitX, StatusBar::kInfoHitY,
	                                     StatusBar::kInfoHitW, StatusBar::kInfoHitH)) {
		while (touch.read(tx, ty)) {
			delay(15);
		}
		InfoUI::run(display, touch, settings, wlan);
		contentDirty = true;
		lastStatusBarMs = 0;
		graph.forceRedraw();
		pingView.forceRedraw();
		statusBar.forceRedraw();
		brandingView.forceRedraw();
	}

	// Ansichtswechsel per Tippen auf die linke (vorige) bzw. rechte
	// (naechste) Bildschirmhaelfte im Inhaltsbereich - zusaetzlich zum
	// automatischen Wechsel im Slide-Modus. Im Static-Modus gibt es keine
	// "naechste" Ansicht (Nutzer hat genau eine feste Quelle gewaehlt,
	// siehe SettingsUI), daher dort ohne Wirkung.
	if (touchedNow && settings.mode() == OperatingMode::Slide && ty >= Layout::kContentTop &&
	    ty < Layout::kContentBottom) {
		bool tappedLeft = tx < DisplayManager::kScreenWidth / 2;
		while (touch.read(tx, ty)) {
			delay(15);
		}
		slideIndex = (slideIndex + (tappedLeft ? kAvailableDataSourceCount - 1 : 1)) % kAvailableDataSourceCount;
		slideLastSwitchMs = millis();
		contentDirty = true;
	}

	bool dhtPolled = sensor.update(settings);
	if (dhtPolled) {
		graph.maybeRecord(sensor.temperatureC(), sensor.humidityPercent());
	}
	bool sensormeterPolled = sensormeterManager.update(settings);
	bool pingPolled = pingManager.update(settings);

	// Anhaltender Ping-Fehler (>1 Min.) oder ueberschrittener Warnschwellwert:
	// LED und Bildschirmhintergrund blinken (1s Taktwechsel) rot
	// (Ueberschreitung/Ausfall) bzw. blau (Unterschreitung), unabhaengig von
	// der gerade aktiven Datenquelle (lastenheft.txt Abschnitt 9 + Nutzer-
	// Erweiterung um Warnschwellwerte, siehe docs/entscheidungen.md).
	AlertInfo alert = computeAlertInfo(sensor, sensormeterManager, pingManager, settings);
	led.update(alert.active, alert.blue);
	bool blinkOn = (millis() / 1000) % 2 == 0;
	uint16_t bgColor = (alert.active && blinkOn) ? (alert.blue ? TFT_BLUE : TFT_RED) : TFT_WHITE;

	DataSource activeSource = currentActiveSource();
	bool showBottomBar = !(settings.mode() == OperatingMode::Static && activeSource == DataSource::Uhrzeit);
	int16_t contentBottom = showBottomBar ? Layout::kContentBottom : DisplayManager::kScreenHeight;

	uint32_t now = millis();
	// Nur Uhrzeit- und Sensormeter-Ansicht brauchen einen Redraw ohne neue
	// Messung (Uhrzeit aendert sich rein zeitgesteuert, Sensormeter rotiert
	// bei mehreren Zielen/Sensoren intern durch die Slides - siehe
	// SensormeterView). Fuer die anderen Quellen erzeugte ein unbedingter
	// 5s-Takt einen sichtbaren, unnoetigen Full-Redraw ohne Datenaenderung
	// (Hardware-Befund: Bildschirm "zitterte" gelegentlich, siehe
	// docs/entscheidungen.md) - dafuer reicht bereits sourceJustPolled/contentDirty.
	bool periodicDue = (activeSource == DataSource::Uhrzeit || activeSource == DataSource::Sensormeter) &&
	                    (now - lastPeriodicRedrawMs >= kPeriodicRedrawIntervalMs);

	bool sourceJustPolled = (activeSource == DataSource::Dht11 && dhtPolled) ||
	                        (activeSource == DataSource::Sensormeter && sensormeterPolled) ||
	                        ((activeSource == DataSource::Ping || activeSource == DataSource::PingTargets) &&
	                         pingPolled);
	// bgColor aendert sich nicht nur bei An-/Abklingen eines Alarms, sondern
	// auch bei jedem 1s-Blink-Taktwechsel waehrend ein Alarm aktiv ist -
	// erzwingt in beiden Faellen ein Neuzeichnen, auch ohne neue Messung.
	static uint16_t lastLoopBgColor = TFT_WHITE;
	bool bgColorChanged = (bgColor != lastLoopBgColor);
	lastLoopBgColor = bgColor;

	if (contentDirty || sourceJustPolled || periodicDue || bgColorChanged) {
		switch (activeSource) {
			case DataSource::Dht11:
				graph.drawFullScreen(display, sensor.temperatureC(), sensor.humidityPercent(),
				                     sensor.hasValidReading(), settings.dhtTempMinC(), settings.dhtTempMaxC(),
				                     settings.dhtHumMinPct(), settings.dhtHumMaxPct(), bgColor);
				break;
			case DataSource::Uhrzeit:
				clockView.draw(display, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Sensormeter:
				sensormeterView.draw(display, sensormeterManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Ping:
				pingView.drawAverage(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::PingTargets:
				pingView.drawTargetList(display, pingManager, Layout::kContentTop, contentBottom, bgColor);
				break;
			case DataSource::Branding:
				brandingView.draw(display, brandingManager, settings, Layout::kContentTop, contentBottom, bgColor);
				break;
		}
		contentDirty = false;
		lastPeriodicRedrawMs = now;
	}

	if (now - lastStatusBarMs >= kStatusBarIntervalMs) {
		lastStatusBarMs = now;
		// alert.extraCount > 0: weitere Kategorien sind ZUSAETZLICH zu
		// alert.source gerade auch ausserhalb der Spec, aber in der
		// schmalen Statusleiste passt nur eine Quelle - "+N" macht
		// wenigstens sichtbar, dass da noch mehr ist (Nutzerwunsch).
		String alertLabel;
		if (alert.active) {
			alertLabel = String(alert.source);
			if (alert.extraCount > 0) {
				alertLabel += " +" + String(alert.extraCount);
			}
		}
		statusBar.draw(display, wlan, sensor.hasValidReading(), sensor.temperatureC(),
		               sensor.humidityPercent(), TimeSync::formatTime(), TimeSync::formatDate(),
		               showBottomBar, bgColor, alertLabel, alert.blue);
	}

	delay(20);
}
