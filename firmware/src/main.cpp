#include <Arduino.h>
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
WebServerManager webServer(settings, backlight, ota, wlan, sensormeterManager, sensor, pingManager, graph);

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

	TimeSync::begin();
	sensor.begin();
	graph.begin();
	pingManager.begin();
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
