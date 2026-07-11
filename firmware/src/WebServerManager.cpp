#include "WebServerManager.h"

#include <ESP32Ping.h>
#include <Update.h>
#include <math.h>
#include <time.h>

#include "AlertEvaluator.h"
#include "TimeSync.h"

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif

namespace {

String escapeHtml(const String &in) {
	String out;
	out.reserve(in.length() + 8);
	for (size_t i = 0; i < in.length(); i++) {
		char c = in[i];
		switch (c) {
			case '&': out += "&amp;"; break;
			case '"': out += "&quot;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			default: out += c;
		}
	}
	return out;
}

// Formularfelder fuer Warnschwellwerte sind optional: leer = kein
// Schwellwert (SettingsManager::kThresholdDisabled bzw. 0 bei Ping-Latenz).
int16_t parseOptionalThreshold(AsyncWebServerRequest *request, const char *name) {
	if (!request->hasParam(name, true)) return SettingsManager::kThresholdDisabled;
	String v = request->getParam(name, true)->value();
	v.trim();
	if (v.isEmpty()) return SettingsManager::kThresholdDisabled;
	return static_cast<int16_t>(v.toInt());
}

uint16_t parseOptionalPingMs(AsyncWebServerRequest *request, const char *name) {
	if (!request->hasParam(name, true)) return 0;
	String v = request->getParam(name, true)->value();
	v.trim();
	if (v.isEmpty()) return 0;
	long ms = v.toInt();
	if (ms < 0) ms = 0;
	if (ms > 65535) ms = 65535;
	return static_cast<uint16_t>(ms);
}

// Sicherheits-Feature: vor dem Uebernehmen einer neu gesetzten statischen
// WLAN-IP prueft dies per Ping, ob im Netz bereits ein Geraet unter dieser
// Adresse antwortet - falls ja, wird die IP-Vergabe abgelehnt statt eine
// Adresskollision zu riskieren. Ping mit count=1 und der
// Bibliotheks-Standard-Wartezeit von 1s - kurz genug, um den
// Async-Webserver-Handler nicht spuerbar zu blockieren (siehe die anderen
// beiden Sensormeter-Projekte, docs/entscheidungen.md).
bool ipRespondsToPing(const IPAddress &ip) {
	if (ip == IPAddress(0, 0, 0, 0)) return false;
	return Ping.ping(ip, 1);
}

// Wartezeit fuer den DHCP-Lease-Test in handleNetworkSave() - laenger als
// der 1s-Ping-Timeout, da eine vollstaendige DHCP-Verhandlung
// (Discover/Offer/Request/Ack) mehr Umlaeufe braucht. Noch nicht auf echter
// Hardware verifiziert, ob das den Async-Webserver-Handler zu lange
// blockiert - bei Auffaelligkeiten (Reboot waehrend des Tests) hier zuerst
// nachsehen.
const unsigned long kDhcpTestTimeoutMs = 8000;

String thresholdAttr(int16_t v) {
	return v == SettingsManager::kThresholdDisabled ? String("") : String(v);
}

String pingMsAttr(uint16_t v) {
	return v == 0 ? String("") : String(v);
}

// Kleines Favicon (Navy-Quadrat mit Orange-Punkt, passend zur uebrigen
// Admin-Guide-Palette) als inline Daten-URI - ohne eigenes Favicon fragen
// Browser automatisch (und erfolglos) danach, jeder Ladevorgang erzeugt
// also einen unnoetigen 404 im Server-Log.
const char *kFaviconLink =
    "<link rel=\"icon\" href=\"data:image/svg+xml,"
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'>"
    "<rect width='32' height='32' rx='6' fill='%230f1f3d'/>"
    "<circle cx='16' cy='16' r='7' fill='%23c8622a'/></svg>\">";

} // namespace

WebServerManager::WebServerManager(SettingsManager &settings, BacklightManager &backlight, OtaManager &ota,
                                    WlanManager &wlan, const SensormeterManager &sensormeterManager,
                                    const SensorManager &sensor, const PingManager &pingManager,
                                    const GraphManager &graph)
    : settings_(settings),
      backlight_(backlight),
      ota_(ota),
      wlan_(wlan),
      sensormeterManager_(sensormeterManager),
      sensor_(sensor),
      pingManager_(pingManager),
      graph_(graph) {}

bool WebServerManager::checkAuth(AsyncWebServerRequest *request) const {
	if (!request->authenticate("admin", settings_.webPassword().c_str())) {
		request->requestAuthentication("Sensormeter Display (Benutzername: admin)");
		return false;
	}
	return true;
}

String WebServerManager::dataSourceOptions(DataSource selected) const {
	String out;
	for (size_t i = 0; i < kAvailableDataSourceCount; i++) {
		DataSource s = kAvailableDataSources[i];
		out += "<option value=\"" + String(i) + "\"";
		if (s == selected) out += " selected";
		out += ">" + escapeHtml(dataSourceLabel(s)) + "</option>";
	}
	return out;
}

// Geteiltes Stylesheet fuer Dashboard UND Einstellungen - Farbpalette/
// Typografie 1:1 aus docs/admin-guide.pdf uebernommen (Navy-Banner #0f1f3d,
// Orange-Akzent #c8622a, warmes Creme #f2f0e9 fuer Tabellenkoepfe/Karten-
// rahmen #e4e1d8), damit Geraete-Weboberflaeche und Dokumentation optisch
// zusammengehoeren (siehe docs/entscheidungen.md). Als eigene Methode statt
// dupliziert in beiden Seiten-Buildern.
String WebServerManager::sharedCss() const {
	return "<style>"
	       "*{box-sizing:border-box}"
	       "body{margin:0;font-family:-apple-system,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif;"
	       "color:#1c2430;background:#f7f5f1}"
	       // .banner hatte urspruenglich per negativem Rand bis an den
	       // Viewport-Rand "gebleedet" und war dadurch breiter als die
	       // Fieldsets/Karten darunter (Nutzer-Feedback: alle Rahmen sollen
	       // dieselbe Breite haben) - jetzt ohne Bleed, exakt so breit wie
	       // .wrap wie alle anderen Boxen.
	       ".wrap{max-width:560px;margin:0 auto;padding:0 14px 28px}"
	       ".banner{background:#0f1f3d;color:#fff;margin:14px 0 18px;padding:18px 16px;border-radius:6px}"
	       ".banner .eyebrow{font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:#9fb3d9;"
	       "margin-bottom:6px}"
	       ".banner h1{margin:0 0 4px;font-size:22px}"
	       ".banner .sub{color:#cdd8ec;font-size:13px}"
	       ".banner .badges{margin-top:10px;display:flex;gap:6px;flex-wrap:wrap}"
	       ".badge{display:inline-block;padding:3px 9px;border-radius:3px;font-size:10.5px;font-weight:600;"
	       "background:#24365c;color:#cdd8ec;text-decoration:none}"
	       ".badge-orange{background:#c8622a;color:#fff}"
	       "fieldset{background:#fff;border:1px solid #e4e1d8;border-radius:6px;padding:12px 16px 16px;"
	       "margin:0 0 14px}"
	       "legend{font-size:14px;color:#8f4a1e;padding:0 4px;font-weight:700}"
	       ".card{background:#fff;border:1px solid #e4e1d8;border-radius:6px;padding:14px 16px;margin-bottom:14px}"
	       ".card h2{font-size:14px;color:#8f4a1e;margin:0 0 10px;padding-bottom:6px;border-bottom:2px solid "
	       "#c8622a}"
	       "label{display:block;margin:8px 0 3px;font-size:13px}"
	       "input,select{width:100%;padding:6px;border:1px solid #d8d4c8;border-radius:4px;font-size:14px}"
	       "button{margin-top:10px;padding:7px 14px;border:none;border-radius:4px;background:#c8622a;color:#fff;"
	       "font-size:13px;font-weight:600;cursor:pointer}"
	       "button.danger{background:#a63d2e}"
	       "button:hover{opacity:.9}"
	       ".row{display:flex;gap:8px;align-items:center;margin:4px 0}"
	       ".row span{flex:1}"
	       // Gruppiert zusammengehoerige Elemente (z.B. Ping-Ziel-IP +
	       // dessen Schwellwert-Formular) mit einer Trennlinie zur naechsten
	       // Gruppe, damit bei mehreren Zielen erkennbar bleibt, was
	       // zusammengehoert (Nutzer-Feedback).
	       ".pingrow-group{border-bottom:1px solid #e4e1d8;padding-bottom:8px;margin-bottom:8px}"
	       "p.hint{font-size:12.5px;color:#6b6559;margin:4px 0}"
	       ".ttable{display:table;width:100%;border-collapse:collapse;font-size:.85em;margin:4px 0}"
	       ".ttable .thead{display:table-row;font-weight:700;background:#f2f0e9}"
	       ".ttable .thead>div{display:table-cell;border:1px solid #e4e1d8;padding:5px 4px;text-align:center}"
	       ".ttable form{display:table-row}"
	       ".ttable form>div{display:table-cell;border:1px solid #e4e1d8;padding:4px;vertical-align:middle}"
	       ".ttable input{padding:3px;margin:0;text-align:center}"
	       ".ttable button{width:auto;margin:0 2px;padding:4px 8px;font-size:.9em}"
	       "table.dtable{width:100%;border-collapse:collapse;font-size:13.5px;margin:2px 0}"
	       "table.dtable th,table.dtable td{border:1px solid #e4e1d8;padding:5px 6px;text-align:left}"
	       "table.dtable th{background:#f2f0e9}"
	       ".value{font-size:22px;font-weight:700}"
	       ".unit{font-size:13px;color:#6b6559;font-weight:400;margin-left:1px}"
	       ".pill{display:inline-block;padding:2px 8px;border-radius:10px;font-size:11.5px;font-weight:700}"
	       ".pill-ok{background:#dcefe1;color:#1f7a44}"
	       ".pill-err{background:#fbeeec;color:#a63d2e}"
	       ".pill-wait{background:#eee;color:#777}"
	       ".alertbar{border-left:4px solid #a63d2e;background:#fbeeec;padding:10px 14px;margin-bottom:14px;"
	       "border-radius:0 4px 4px 0}"
	       ".alertbar.blue{border-left-color:#2a5ba0;background:#eaf1fb}"
	       ".alertbar .t{font-weight:700;font-size:13px;text-transform:uppercase;letter-spacing:.03em;"
	       "color:#a63d2e}"
	       ".alertbar.blue .t{color:#2a5ba0}"
	       "footer.pagefoot{font-size:12px;color:#8b8577;text-align:center;margin-top:6px}"
	       "footer.pagefoot a{color:#8f4a1e;text-decoration:none}"
	       ".wifibars{display:inline-flex;align-items:flex-end;gap:2px;height:13px;vertical-align:middle}"
	       ".wifibars .bar{width:4px;background:#d8d4c8;border-radius:1px}"
	       ".wifibars .bar.on{background:#1c2430}"
	       ".wifibars .bar.h1{height:5px}"
	       ".wifibars .bar.h2{height:9px}"
	       ".wifibars .bar.h3{height:13px}"
	       ".toast-ok{background:#dcefe1;color:#1f7a44;border-left:4px solid #1f7a44;padding:8px 14px;"
	       "border-radius:0 4px 4px 0;margin-bottom:14px;font-size:13px;font-weight:600}"
	       "</style>";
}

String WebServerManager::bannerHtml(const String &eyebrow, const String &subLine, const String &linkHref,
                                     const String &linkLabel) const {
	String html;
	html += "<div class=\"banner\"><div class=\"eyebrow\">" + escapeHtml(eyebrow) + "</div>";
	html += "<h1>" + escapeHtml(settings_.deviceName()) + "</h1>";
	if (!subLine.isEmpty()) {
		html += "<div class=\"sub\">" + escapeHtml(subLine) + "</div>";
	}
	html += "<div class=\"badges\"><span class=\"badge badge-orange\">v" DEVICE_FIRMWARE_VERSION "</span>";
	html += "<span class=\"badge\">" + WiFi.localIP().toString() + "</span>";
	if (!linkHref.isEmpty()) {
		html += "<a class=\"badge\" href=\"" + linkHref + "\">" + escapeHtml(linkLabel) + "</a>";
	}
	html += "</div></div>";
	return html;
}

// WLAN-Empfang als drei ansteigende Balken (gefuellt bis `bars`), analog
// zum Symbol in der Touch-UI-Statusleiste (StatusBar::drawWifiIcon()) -
// statt reinem "X/3"-Text.
String WebServerManager::wifiBarsHtml(int8_t bars) const {
	String html = "<span class=\"wifibars\">";
	for (int8_t b = 0; b < 3; b++) {
		html += "<span class=\"bar h" + String(b + 1) + (b < bars ? " on" : "") + "\"></span>";
	}
	html += "</span>";
	return html;
}

// Verlaufsgraph fuer den internen DHT11-Sensor als inline SVG (kein JS,
// kein Canvas nötig) - analog zu GraphManager::drawGraph() auf dem
// Touch-Display: Temperatur rot, Feuchte blau, Warnschwellwerte gestrichelt
// (auf den Plotbereich geklemmt, falls ausserhalb des sichtbaren Bereichs).
String WebServerManager::dhtGraphSvg() const {
	size_t n = graph_.entryCount();
	if (n < 2) {
		return "<p class=\"hint\">Noch nicht genug Messwerte für den Verlauf</p>";
	}

	int16_t tempMin = graph_.entryTempC(0), tempMax = tempMin;
	int16_t humMin = graph_.entryHumidityPct(0), humMax = humMin;
	for (size_t i = 1; i < n; i++) {
		int16_t t = graph_.entryTempC(i), h = graph_.entryHumidityPct(i);
		tempMin = min(tempMin, t);
		tempMax = max(tempMax, t);
		humMin = min(humMin, h);
		humMax = max(humMax, h);
	}
	if (tempMax == tempMin) tempMax++;
	if (humMax == humMin) humMax++;

	constexpr int kW = 520, kH = 150, kMarginX = 34, kMarginY = 16;
	auto px = [&](size_t i) -> int {
		return kMarginX + static_cast<int>(static_cast<long>(i) * (kW - 2 * kMarginX) / static_cast<long>(n - 1));
	};
	auto pyTemp = [&](int16_t v) -> int {
		return kMarginY + (kH - 2 * kMarginY) -
		       static_cast<int>(static_cast<long>(v - tempMin) * (kH - 2 * kMarginY) / (tempMax - tempMin));
	};
	auto pyHum = [&](int16_t v) -> int {
		return kMarginY + (kH - 2 * kMarginY) -
		       static_cast<int>(static_cast<long>(v - humMin) * (kH - 2 * kMarginY) / (humMax - humMin));
	};
	auto clampY = [&](int y) -> int { return max(kMarginY, min(kH - kMarginY, y)); };

	String pointsTemp, pointsHum;
	for (size_t i = 0; i < n; i++) {
		pointsTemp += String(px(i)) + "," + String(pyTemp(graph_.entryTempC(i))) + " ";
		pointsHum += String(px(i)) + "," + String(pyHum(graph_.entryHumidityPct(i))) + " ";
	}

	String svg = "<svg viewBox=\"0 0 " + String(kW) + " " + String(kH) + "\" style=\"width:100%;height:auto\">";
	svg += "<polyline points=\"" + pointsTemp + "\" fill=\"none\" stroke=\"#a63d2e\" stroke-width=\"2\"/>";
	svg += "<polyline points=\"" + pointsHum + "\" fill=\"none\" stroke=\"#2a5ba0\" stroke-width=\"2\"/>";

	int16_t tMin = settings_.dhtTempMinC(), tMax = settings_.dhtTempMaxC();
	int16_t hMin = settings_.dhtHumMinPct(), hMax = settings_.dhtHumMaxPct();
	auto thresholdLine = [&](int y, const char *color) -> String {
		return "<line x1=\"" + String(kMarginX) + "\" y1=\"" + String(y) + "\" x2=\"" + String(kW - kMarginX) +
		       "\" y2=\"" + String(y) + "\" stroke=\"" + color + "\" stroke-width=\"1\" stroke-dasharray=\"4,3\"/>";
	};
	if (tMin != SettingsManager::kThresholdDisabled) svg += thresholdLine(clampY(pyTemp(tMin)), "#a63d2e");
	if (tMax != SettingsManager::kThresholdDisabled) svg += thresholdLine(clampY(pyTemp(tMax)), "#a63d2e");
	if (hMin != SettingsManager::kThresholdDisabled) svg += thresholdLine(clampY(pyHum(hMin)), "#2a5ba0");
	if (hMax != SettingsManager::kThresholdDisabled) svg += thresholdLine(clampY(pyHum(hMax)), "#2a5ba0");

	// Hochachsen in 1°/1%-Schritten beschriftet (links Temperatur, rechts
	// Feuchte) - kein fester Rundwert-Raster (0/5/10/...), sondern das
	// "Sliding Window" aus tempMin/tempMax bzw. humMin/humMax selbst:
	// dadurch landen die tatsaechlich erfassten Maximal-/Minimalwerte
	// IMMER exakt auf einem beschrifteten Strich, nicht zwischen zwei
	// Rundwert-Stufen (Nutzeranforderung).
	svg += "<text x=\"2\" y=\"12\" font-size=\"10\" fill=\"#a63d2e\">°C</text>";
	svg += "<text x=\"" + String(kW - 2) + "\" y=\"12\" font-size=\"10\" fill=\"#2a5ba0\" text-anchor=\"end\">%</text>";
	for (int16_t v = tempMin; v <= tempMax; v++) {
		int y = pyTemp(v);
		svg += "<line x1=\"" + String(kMarginX - 4) + "\" y1=\"" + String(y) + "\" x2=\"" + String(kMarginX) +
		       "\" y2=\"" + String(y) + "\" stroke=\"#a63d2e\" stroke-width=\"1\"/>";
		svg += "<text x=\"" + String(kMarginX - 6) + "\" y=\"" + String(y + 3) +
		       "\" font-size=\"9\" fill=\"#a63d2e\" text-anchor=\"end\">" + String(v) + "</text>";
	}
	for (int16_t v = humMin; v <= humMax; v++) {
		int y = pyHum(v);
		svg += "<line x1=\"" + String(kW - kMarginX) + "\" y1=\"" + String(y) + "\" x2=\"" + String(kW - kMarginX + 4) +
		       "\" y2=\"" + String(y) + "\" stroke=\"#2a5ba0\" stroke-width=\"1\"/>";
		svg += "<text x=\"" + String(kW - kMarginX + 6) + "\" y=\"" + String(y + 3) +
		       "\" font-size=\"9\" fill=\"#2a5ba0\" text-anchor=\"start\">" + String(v) + "</text>";
	}

	if (TimeSync::isValid()) {
		struct tm tmStart, tmEnd;
		time_t tsStart = graph_.entryTs(0), tsEnd = graph_.entryTs(n - 1);
		localtime_r(&tsStart, &tmStart);
		localtime_r(&tsEnd, &tmEnd);
		char bufS[6], bufE[6];
		snprintf(bufS, sizeof(bufS), "%02d:%02d", tmStart.tm_hour, tmStart.tm_min);
		snprintf(bufE, sizeof(bufE), "%02d:%02d", tmEnd.tm_hour, tmEnd.tm_min);
		svg += "<text x=\"" + String(kMarginX) + "\" y=\"" + String(kH - 2) +
		       "\" font-size=\"10\" fill=\"#1c2430\">" + bufS + "</text>";
		svg += "<text x=\"" + String(kW - kMarginX) + "\" y=\"" + String(kH - 2) +
		       "\" font-size=\"10\" fill=\"#1c2430\" text-anchor=\"end\">" + bufE + "</text>";
	}

	svg += "</svg>";
	return svg;
}

// Oeffentliches Status-Dashboard: kein Login, keine Formulare, nur
// Anzeige - Auto-Refresh alle 30s per Meta-Tag (kein JS nötig). Nutzt
// dieselbe computeAlertInfo()-Logik wie main.cpp (siehe AlertEvaluator.h),
// damit Touch-UI-Alarm und Dashboard-Alarm nie auseinanderlaufen koennen.
String WebServerManager::buildDashboardPage() const {
	AlertInfo alert = computeAlertInfo(sensor_, sensormeterManager_, pingManager_, settings_);

	String html;
	html.reserve(4500);
	html += "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">";
	html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
	html += "<meta http-equiv=\"refresh\" content=\"30\">";
	html += kFaviconLink;
	html += "<title>" + escapeHtml(settings_.deviceName()) + " – Status</title>";
	html += sharedCss();
	html += "</head><body><div class=\"wrap\">";
	String subLine = "Aktualisiert automatisch alle 30s";
	String lastSync = TimeSync::formatLastSync();
	if (!lastSync.isEmpty()) {
		subLine += " · Letzter NTP-Sync: " + lastSync;
	}
	html += bannerHtml("Sensormeter Display · Live-Status", subLine, "/settings", "Einstellungen");

	if (alert.active) {
		html += "<div class=\"alertbar" + String(alert.blue ? " blue" : "") + "\">";
		html += "<div class=\"t\">Warnung: " + escapeHtml(String(alert.source)) + "</div>";
		html += "<div>" +
		        String(alert.blue ? "Warnschwellwert unterschritten" : "Warnschwellwert überschritten bzw. Ausfall");
		// Weitere, gleichzeitig aktive Kategorien nicht stillschweigend
		// verdecken (Nutzerwunsch) - Statusleiste zeigt ohnehin nur eine
		// Quelle, hier ist mehr Platz fuer einen erklaerenden Hinweis.
		if (alert.extraCount > 0) {
			html += " — außerdem " + String(alert.extraCount) +
			        (alert.extraCount == 1 ? " weitere Quelle betroffen" : " weitere Quellen betroffen");
		}
		html += "</div></div>";
	}

	// --- DHT11 (intern) ---
	html += "<div class=\"card\"><h2>Intern (DHT11)</h2>";
	if (sensor_.hasValidReading()) {
		html += "<span class=\"value\">" + String(static_cast<int>(lroundf(sensor_.temperatureC()))) +
		        "<span class=\"unit\">°C</span></span>&nbsp;&nbsp;";
		html += "<span class=\"value\">" + String(static_cast<int>(lroundf(sensor_.humidityPercent()))) +
		        "<span class=\"unit\">%</span></span>";
		// Zeitpunkt der Werterfassung in derselben Zeile wie die Werte
		// selbst (Nutzerwunsch) - nicht die aktuelle Uhrzeit, sondern
		// SensorManager::lastReadTime() (Zeitpunkt der letzten tatsaechlich
		// plausiblen Messung, siehe SensorManager.cpp).
		time_t readTs = sensor_.lastReadTime();
		if (readTs != 0) {
			struct tm t;
			localtime_r(&readTs, &t);
			char buf[6];
			snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
			html += " <span class=\"hint\">(Stand " + String(buf) + " Uhr)</span>";
		}
	} else {
		html += "<p class=\"hint\">Sensor --</p>";
	}
	html += dhtGraphSvg();
	html += "</div>";

	// --- WLAN ---
	html += "<div class=\"card\"><h2>WLAN</h2><p>";
	if (wlan_.isConnected()) {
		html += escapeHtml(WiFi.SSID()) + " &middot; " + wifiBarsHtml(wlan_.signalBars()) + " &middot; " +
		        WiFi.localIP().toString();
	} else {
		html += "Nicht verbunden";
	}
	html += "</p></div>";

	// --- Sensormeter-Ziele ---
	html += "<div class=\"card\"><h2>Sensormeter-Ziele</h2>";
	size_t smCount = sensormeterManager_.targetCount();
	if (smCount == 0) {
		html += "<p class=\"hint\">Keine Ziele konfiguriert</p>";
	} else {
		html += "<table class=\"dtable\"><tr><th>Ziel</th><th>Temperatur</th><th>Feuchte</th></tr>";
		for (size_t i = 0; i < smCount; i++) {
			if (!sensormeterManager_.isResolved(i)) {
				html += "<tr><td>" + escapeHtml(settings_.sensormeterTargetIp(i)) +
				        "</td><td colspan=\"2\">wird aufgelöst ...</td></tr>";
				continue;
			}
			uint8_t sensorCount = sensormeterManager_.isPro(i) ? 2 : 1;
			for (uint8_t s = 0; s < sensorCount; s++) {
				String name = sensormeterManager_.systemName(i);
				String sensorName = sensormeterManager_.sensorName(i, s);
				if (!sensorName.isEmpty()) name += " (" + sensorName + ")";
				html += "<tr><td>" + escapeHtml(name) + "</td>";
				if (sensormeterManager_.sensorValid(i, s)) {
					char buf[8];
					snprintf(buf, sizeof(buf), "%.1f", sensormeterManager_.sensorTempC(i, s));
					html += "<td>" + String(buf) + "°C</td>";
					snprintf(buf, sizeof(buf), "%.1f", sensormeterManager_.sensorHumidityPct(i, s));
					html += "<td>" + String(buf) + "%</td>";
				} else {
					html += "<td>--</td><td>--</td>";
				}
				html += "</tr>";
			}
		}
		html += "</table>";
	}
	html += "</div>";

	// --- Ping ---
	html += "<div class=\"card\"><h2>Ping</h2><p>google.com: ";
	if (pingManager_.hasGoogleReading()) {
		html += String(static_cast<int>(lroundf(pingManager_.googleAverageLatencyMs()))) + "ms ";
		html += pingManager_.isFailingOver1Min() ? "<span class=\"pill pill-err\">Ausfall</span>"
		                                          : "<span class=\"pill pill-ok\">OK</span>";
	} else {
		html += "<span class=\"pill pill-wait\">noch keine Antwort</span>";
	}
	html += "</p>";
	size_t pingCount = pingManager_.targetCount();
	if (pingCount > 0) {
		html += "<table class=\"dtable\"><tr><th>Ziel</th><th>Status</th><th>Latenz</th></tr>";
		for (size_t i = 0; i < pingCount; i++) {
			bool checked = pingManager_.targetChecked(i);
			bool ok = pingManager_.targetOk(i);
			html += "<tr><td>" + escapeHtml(pingManager_.targetIp(i)) + "</td><td>";
			html += !checked ? "<span class=\"pill pill-wait\">...</span>"
			                 : (ok ? "<span class=\"pill pill-ok\">OK</span>"
			                       : "<span class=\"pill pill-err\">Fehler</span>");
			html += "</td><td>";
			html += pingManager_.targetHasLatency(i)
			            ? String(static_cast<int>(lroundf(pingManager_.targetLatencyMs(i)))) + "ms"
			            : String("--");
			html += "</td></tr>";
		}
		html += "</table>";
	}
	html += "</div>";

	html += "<footer class=\"pagefoot\">Sensormeter Display v" DEVICE_FIRMWARE_VERSION " &middot; "
	        "<a href=\"/settings\">Einstellungen (Login erforderlich)</a></footer>";
	html += "</div></body></html>";
	return html;
}

String WebServerManager::buildConfigExport() const {
	String out;
	out.reserve(2200);
	out += "# Sensormeter Display - Konfigurationsexport\n";
	out += "systemname=" + settings_.deviceName() + "\n";
	out += "betriebsmodus=" + String(settings_.mode() == OperatingMode::Slide ? "slide" : "static") + "\n";
	if (settings_.mode() == OperatingMode::Slide) {
		out += "slideIntervalSec=" + String(settings_.slideIntervalSec()) + "\n";
	} else {
		out += "staticSource=" + String(dataSourceLabel(settings_.staticSource())) + "\n";
	}
	out += "helligkeitProzent=" + String(settings_.brightnessPercent()) + "\n";

	out += "\n[Kalibrierung intern]\n";
	out += "dhtTempOffsetC=" + String(settings_.dhtTempOffsetC()) + "\n";
	out += "dhtHumOffsetPct=" + String(settings_.dhtHumOffsetPct()) + "\n";
	time_t calTs = settings_.dhtOffsetSetTime();
	if (calTs == 0) {
		out += "zuletztKalibriert=noch nie\n";
	} else {
		struct tm t;
		localtime_r(&calTs, &t);
		char buf[20];
		snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour,
		         t.tm_min);
		out += "zuletztKalibriert=" + String(buf) + "\n";
	}

	out += "\n[Warnschwellwerte intern]\n";
	out += "dhtTempMinC=" + thresholdAttr(settings_.dhtTempMinC()) + "\n";
	out += "dhtTempMaxC=" + thresholdAttr(settings_.dhtTempMaxC()) + "\n";
	out += "dhtHumMinPct=" + thresholdAttr(settings_.dhtHumMinPct()) + "\n";
	out += "dhtHumMaxPct=" + thresholdAttr(settings_.dhtHumMaxPct()) + "\n";

	out += "\n[Sensormeter-Ziele]\n";
	out += "snmpCommunity=" + settings_.sensormeterCommunity() + "\n";
	for (size_t i = 0; i < settings_.sensormeterTargetCount(); i++) {
		out += "ziel" + String(i + 1) + ".ip=" + settings_.sensormeterTargetIp(i) + "\n";
		for (uint8_t s = 0; s < SettingsManager::kMaxSensorsPerTarget; s++) {
			out += "ziel" + String(i + 1) + ".sensor" + String(s + 1) +
			       ".tempMinC=" + thresholdAttr(settings_.sensormeterTempMinC(i, s)) + "\n";
			out += "ziel" + String(i + 1) + ".sensor" + String(s + 1) +
			       ".tempMaxC=" + thresholdAttr(settings_.sensormeterTempMaxC(i, s)) + "\n";
			out += "ziel" + String(i + 1) + ".sensor" + String(s + 1) +
			       ".humMinPct=" + thresholdAttr(settings_.sensormeterHumMinPct(i, s)) + "\n";
			out += "ziel" + String(i + 1) + ".sensor" + String(s + 1) +
			       ".humMaxPct=" + thresholdAttr(settings_.sensormeterHumMaxPct(i, s)) + "\n";
		}
	}

	out += "\n[Ping-Ziele]\n";
	out += "google.maxLatencyMs=" + pingMsAttr(settings_.googlePingMaxLatencyMs()) + "\n";
	for (size_t i = 0; i < settings_.pingTargetCount(); i++) {
		out += "ziel" + String(i + 1) + ".ip=" + settings_.pingTargetIp(i) + "\n";
		out += "ziel" + String(i + 1) + ".maxLatencyMs=" + pingMsAttr(settings_.pingMaxLatencyMs(i)) + "\n";
	}

	out += "\n[Netzwerk]\n";
	out += "ip=" + WiFi.localIP().toString() + "\n";
	out += "statischeIp=" + String(wlan_.hasStaticIp() ? "ja" : "nein") + "\n";

	out += "\nFirmware=" DEVICE_FIRMWARE_VERSION "\n";
	return out;
}

String WebServerManager::buildSettingsPage(bool saved) const {
	bool isSlide = settings_.mode() == OperatingMode::Slide;

	String html;
	html.reserve(4600);
	html += "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">";
	html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
	html += kFaviconLink;
	html += "<title>" + escapeHtml(settings_.deviceName()) + " – Einstellungen</title>";
	html += sharedCss();
	html += "</head><body><div class=\"wrap\">";
	html += bannerHtml("Einstellungen (angemeldet)", "", "/", "Zur Statusseite");
	// Bisher nur stiller Redirect nach dem Speichern - kein Hinweis, ob es
	// tatsaechlich geklappt hat (Nutzer-Feedback). Ohne JS geht das nur per
	// Redirect-Parameter (siehe handleSave() etc.: "/settings?saved=1").
	if (saved) {
		html += "<div class=\"toast-ok\">Gespeichert.</div>";
	}

	// --- Allgemein + Betriebsmodus + Helligkeit + Sensormeter (ein Formular) ---
	html += "<form method=\"POST\" action=\"/save\">";
	html += "<fieldset><legend>Allgemein</legend>";
	html += "<label>Systemname</label><input name=\"name\" value=\"" + escapeHtml(settings_.deviceName()) + "\">";
	html += "<label>Web-Passwort</label><input name=\"webPw\" type=\"password\" value=\"" +
	        escapeHtml(settings_.webPassword()) + "\">";
	html += "</fieldset>";

	html += "<fieldset><legend>Betriebsmodus</legend>";
	html += "<label><input type=\"radio\" name=\"mode\" value=\"slide\"";
	if (isSlide) html += " checked";
	html += "> Slide (zyklischer Wechsel)</label>";
	html += "<label>Slide-Intervall (Sekunden, 5-60)</label>";
	html += "<input name=\"slideInterval\" type=\"number\" min=\"5\" max=\"60\" step=\"5\" value=\"" +
	        String(settings_.slideIntervalSec()) + "\">";
	html += "<label><input type=\"radio\" name=\"mode\" value=\"static\"";
	if (!isSlide) html += " checked";
	html += "> Static (feste Datenquelle)</label>";
	html += "<label>Datenquelle</label><select name=\"staticSource\">";
	html += dataSourceOptions(settings_.staticSource());
	html += "</select>";
	html += "</fieldset>";

	html += "<fieldset><legend>Helligkeit</legend>";
	html += "<input name=\"brightness\" type=\"number\" min=\"0\" max=\"100\" value=\"" +
	        String(settings_.brightnessPercent()) + "\"> %";
	html += "</fieldset>";

	html += "<fieldset><legend>Sensormeter (SNMP)</legend>";
	html += "<label>Community (gilt fuer alle Ziele)</label><input name=\"smCommunity\" value=\"" +
	        escapeHtml(settings_.sensormeterCommunity()) + "\">";
	html += "</fieldset>";

	html += "<fieldset><legend>Kalibrierung (intern)</legend>";
	html += "<p class=\"hint\" style=\"margin-top:0\">Korrigiert den DHT11-Messwert um einen festen Versatz, "
	        "z.B. wenn er von einem Referenzsensor am gleichen Standort abweicht. Wirkt auf Touch-Display, "
	        "Webseiten und Warnschwellwerte gleichermaßen.</p>";
	html += "<label>Temperatur-Korrektur (°C, ganzzahlig)</label>";
	html += "<input name=\"dhtTempOffset\" type=\"number\" step=\"1\" value=\"" +
	        String(settings_.dhtTempOffsetC()) + "\">";
	html += "<label>Feuchte-Korrektur (%, ganzzahlig)</label>";
	html += "<input name=\"dhtHumOffset\" type=\"number\" step=\"1\" value=\"" +
	        String(settings_.dhtHumOffsetPct()) + "\">";
	{
		time_t calTs = settings_.dhtOffsetSetTime();
		String calText = "Zuletzt kalibriert: ";
		if (calTs == 0) {
			calText += "noch nie";
		} else {
			struct tm t;
			localtime_r(&calTs, &t);
			char buf[20];
			snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900,
			         t.tm_hour, t.tm_min);
			calText += buf;
		}
		html += "<p class=\"hint\">" + calText + "</p>";
	}
	html += "</fieldset>";

	html += "<fieldset><legend>Warnschwellwerte (allgemein)</legend>";
	html += "<p class=\"hint\" style=\"margin-top:0\">Leer lassen = kein Schwellwert. Bei Über-/Unterschreitung "
	        "blinkt der Bildschirm rot/blau (siehe Statusseite).</p>";
	html += "<label>DHT11 intern: Temperatur min/max (°C)</label>";
	html += "<div class=\"row\"><input name=\"dhtTempMin\" type=\"number\" value=\"" +
	        thresholdAttr(settings_.dhtTempMinC()) + "\"><input name=\"dhtTempMax\" type=\"number\" value=\"" +
	        thresholdAttr(settings_.dhtTempMaxC()) + "\"></div>";
	html += "<label>DHT11 intern: Luftfeuchte min/max (%)</label>";
	html += "<div class=\"row\"><input name=\"dhtHumMin\" type=\"number\" value=\"" +
	        thresholdAttr(settings_.dhtHumMinPct()) + "\"><input name=\"dhtHumMax\" type=\"number\" value=\"" +
	        thresholdAttr(settings_.dhtHumMaxPct()) + "\"></div>";
	html += "<label>Ping google.com: Max-Latenz (ms)</label>";
	html += "<input name=\"googlePingMax\" type=\"number\" min=\"0\" value=\"" +
	        pingMsAttr(settings_.googlePingMaxLatencyMs()) + "\">";
	html += "</fieldset>";

	html += "<button type=\"submit\">Speichern</button>";
	html += "</form>";

	// --- Sensormeter-Ziele (eigene, dynamische Liste) - als Tabelle mit
	// beschrifteten Spalten, da bei mehreren Zielen sonst nicht erkennbar
	// war, welches Min/Max-Feld zu welchem Ziel/Sensor gehoert (Nutzer-
	// Feedback). Jede Zeile ist EIN Formular mit zwei Buttons (Speichern
	// per Standard-action, Entfernen per formaction) statt zwei getrennten
	// Formularen - vermeidet ungueltiges <form>-in-<form>-Nesting bei
	// gleichzeitig tabellarischem Layout. PRO-Ziele (2 Sensoren) bekommen
	// zwei Zeilen mit eigenen Schwellwerten statt einem gemeinsamen Satz.
	html += "<fieldset><legend>Sensormeter-Ziele (max. " + String(SettingsManager::kMaxSensormeterTargets) +
	        ", mind. 1)</legend>";
	html += "<p class=\"hint\" style=\"margin-top:0\">IP-Feld direkt ändern + Speichern, um ein Ziel neu "
	        "zu setzen (auch das letzte verbleibende, nicht entfernbare Ziel).</p>";
	size_t smCount = settings_.sensormeterTargetCount();
	html += "<div class=\"ttable\"><div class=\"thead\"><div>Ziel</div><div>Temp min<br>(°C)</div>"
	        "<div>Temp max<br>(°C)</div><div>Feuchte min<br>(%)</div><div>Feuchte max<br>(%)</div><div></div></div>";
	for (size_t i = 0; i < smCount; i++) {
		bool pro = sensormeterManager_.isResolved(i) && sensormeterManager_.isPro(i);
		uint8_t sensorRows = pro ? SettingsManager::kMaxSensorsPerTarget : 1;
		for (uint8_t s = 0; s < sensorRows; s++) {
			html += "<form method=\"POST\" action=\"/sensormeter/thresholds\">";
			html += "<input type=\"hidden\" name=\"i\" value=\"" + String(i) + "\">";
			html += "<input type=\"hidden\" name=\"s\" value=\"" + String(s) + "\">";
			// Nur die erste Sensor-Zeile bietet ein editierbares IP-Feld -
			// die IP gehoert dem ganzen Ziel, nicht dem einzelnen Sensor,
			// daher hier nicht pro Zeile duplizieren (siehe Entfernen-Button
			// unten, gleiches Prinzip).
			if (s == 0) {
				html += "<div><input name=\"ip\" value=\"" + escapeHtml(settings_.sensormeterTargetIp(i)) + "\">";
				if (pro) {
					String sensorName = sensormeterManager_.sensorName(i, 0);
					html += "<div style=\"font-size:.75em;color:#6b6559\">Sensor 1" +
					        (sensorName.isEmpty() ? String("") : (": " + escapeHtml(sensorName))) + "</div>";
				}
				html += "</div>";
			} else {
				String label = escapeHtml(settings_.sensormeterTargetIp(i));
				String sensorName = sensormeterManager_.sensorName(i, s);
				label += " (" + escapeHtml(sensorName.isEmpty() ? ("Sensor " + String(s + 1)) : sensorName) + ")";
				html += "<div>" + label + "</div>";
			}
			html += "<div><input name=\"tempMin\" type=\"number\" value=\"" +
			        thresholdAttr(settings_.sensormeterTempMinC(i, s)) + "\"></div>";
			html += "<div><input name=\"tempMax\" type=\"number\" value=\"" +
			        thresholdAttr(settings_.sensormeterTempMaxC(i, s)) + "\"></div>";
			html += "<div><input name=\"humMin\" type=\"number\" value=\"" +
			        thresholdAttr(settings_.sensormeterHumMinPct(i, s)) + "\"></div>";
			html += "<div><input name=\"humMax\" type=\"number\" value=\"" +
			        thresholdAttr(settings_.sensormeterHumMaxPct(i, s)) + "\"></div>";
			html += "<div><button type=\"submit\">Speichern</button>";
			// Entfernen bezieht sich auf das ganze Ziel (beide Sensoren), nicht
			// nur diese Zeile - Button daher nur bei der ersten Sensor-Zeile.
			if (smCount > 1 && s == 0) {
				html += "<button type=\"submit\" class=\"danger\" formaction=\"/sensormeter/remove\">Entfernen</button>";
			}
			html += "</div></form>";
		}
	}
	html += "</div>"; // .ttable
	if (smCount < SettingsManager::kMaxSensormeterTargets) {
		html += "<form method=\"POST\" action=\"/sensormeter/add\">";
		html += "<label>Neues Ziel (IP-Adresse)</label><input name=\"ip\">";
		html += "<button type=\"submit\">Hinzufuegen</button></form>";
	}
	html += "</fieldset>";

	// --- Ping-Ziele (eigene, dynamische Liste - separate Formulare) ---
	html += "<fieldset><legend>Ping-Ziele (max. " + String(SettingsManager::kMaxPingTargets) + ")</legend>";
	// IP-Zeile und Schwellwert-Formular gehoeren zusammen zum selben Ziel,
	// waren aber ohne visuelle Klammer nur zwei aufeinanderfolgende Bloecke -
	// bei mehreren Zielen nicht erkennbar, wo eins endet und das naechste
	// beginnt (Nutzer-Feedback). Jetzt in einer gemeinsamen Gruppe mit
	// Trennlinie zum naechsten Ziel.
	for (size_t i = 0; i < settings_.pingTargetCount(); i++) {
		html += "<div class=\"pingrow-group\">";
		html += "<div class=\"row\"><span>" + escapeHtml(settings_.pingTargetIp(i)) + "</span>";
		html += "<form method=\"POST\" action=\"/ping/remove\" style=\"margin:0\">";
		html += "<input type=\"hidden\" name=\"i\" value=\"" + String(i) + "\">";
		html += "<button type=\"submit\" class=\"danger\">Entfernen</button></form></div>";
		html += "<form method=\"POST\" action=\"/ping/threshold\" "
		        "style=\"display:flex;gap:4px;align-items:center;margin:2px 0\">";
		html += "<input type=\"hidden\" name=\"i\" value=\"" + String(i) + "\">";
		html += "<span class=\"hint\">Max-Latenz (ms):</span>";
		html += "<input name=\"maxMs\" type=\"number\" min=\"0\" style=\"width:90px\" value=\"" +
		        pingMsAttr(settings_.pingMaxLatencyMs(i)) + "\">";
		html += "<button type=\"submit\" style=\"width:auto\">Speichern</button></form>";
		html += "</div>"; // .pingrow-group
	}
	if (settings_.pingTargetCount() < SettingsManager::kMaxPingTargets) {
		html += "<form method=\"POST\" action=\"/ping/add\">";
		html += "<label>Neues Ziel (IP-Adresse)</label><input name=\"ip\">";
		html += "<button type=\"submit\">Hinzufuegen</button></form>";
	}
	html += "</fieldset>";

	// --- Netzwerk (statische IP, sonst DHCP) - eigenes Formular mit
	// eigenem Endpunkt, da das Anwenden neuer Netzwerkeinstellungen einen
	// Neustart braucht (WiFi.config() wirkt erst bei der naechsten
	// Verbindung, nicht auf eine bereits bestehende) ---
	{
		bool staticIp = wlan_.hasStaticIp();
		IPAddress ip, gateway, subnet;
		wlan_.loadStaticIp(ip, gateway, subnet);
		html += "<fieldset><legend>Netzwerk</legend>";
		html += "<p class=\"hint\">Aktuelle IP: " + WiFi.localIP().toString() + " (" +
		        (staticIp ? "statisch" : "DHCP") + ")</p>";
		html += "<form method=\"POST\" action=\"/network/save\">";
		html += "<label><input type=\"radio\" name=\"ipMode\" value=\"dhcp\"" + String(staticIp ? "" : " checked") +
		        "> Automatisch (DHCP)</label>";
		html += "<label><input type=\"radio\" name=\"ipMode\" value=\"static\"" +
		        String(staticIp ? " checked" : "") + "> Statisch</label>";
		html += "<label>IP-Adresse</label><input name=\"ip\" value=\"" +
		        (staticIp ? ip.toString() : String("")) + "\">";
		html += "<label>Gateway</label><input name=\"gateway\" value=\"" +
		        (staticIp ? gateway.toString() : String("")) + "\">";
		html += "<label>Subnetzmaske</label><input name=\"subnet\" value=\"" +
		        (staticIp ? subnet.toString() : String("255.255.255.0")) + "\">";
		html += "<button type=\"submit\">Speichern (Geraet startet neu)</button>";
		html += "<p class=\"hint\">Prueft vor der Uebernahme, ob die Verbindung tatsaechlich moeglich ist - bei "
		        "\"Automatisch (DHCP)\" durch einen echten Verbindungsversuch (bis zu 8s, nur bei erhaltener "
		        "Lease wird uebernommen), bei \"Statisch\" per Ping (Adresse darf nicht bereits belegt sein). "
		        "Erst bei Erfolg werden die Netzwerkfelder gespeichert und das Geraet neu gestartet.</p>";
		html += "</form></fieldset>";
	}

	// --- Firmware-Update ---
	html += "<fieldset><legend>Firmware-Update</legend>";
	html += "<p class=\"hint\">Aktuell installiert: " DEVICE_FIRMWARE_VERSION "</p>";
	html += "<form method=\"POST\" action=\"/ota/upload\" enctype=\"multipart/form-data\">";
	html += "<input type=\"file\" name=\"file\" accept=\".bin\">";
	html += "<button type=\"submit\">.bin hochladen</button></form>";
	html += "<p><a href=\"https://github.com/peterhagelhof7-cmd/sensormeter-display/releases\">"
	        "Releases auf GitHub</a></p>";
	html += "</fieldset>";

	// --- Konfiguration exportieren ---
	html += "<fieldset><legend>Konfiguration</legend>";
	html += "<p class=\"hint\">Alle Einstellungen als Textdatei sichern (Backup/Dokumentation).</p>";
	html += "<a href=\"/settings/export\"><button type=\"button\">Konfiguration herunterladen</button></a>";
	html += "</fieldset>";

	html += "</div></body></html>";
	return html;
}

void WebServerManager::handleSave(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;

	if (request->hasParam("name", true)) {
		settings_.setDeviceName(request->getParam("name", true)->value());
	}
	if (request->hasParam("webPw", true)) {
		settings_.setWebPassword(request->getParam("webPw", true)->value());
	}
	if (request->hasParam("brightness", true)) {
		int b = request->getParam("brightness", true)->value().toInt();
		if (b < 0) b = 0;
		if (b > 100) b = 100;
		backlight_.setPercent(static_cast<uint8_t>(b));
		settings_.setBrightnessPercent(static_cast<uint8_t>(b));
	}
	if (request->hasParam("smCommunity", true)) {
		settings_.setSensormeterCommunity(request->getParam("smCommunity", true)->value());
	}

	int16_t tempOffset = settings_.dhtTempOffsetC();
	int16_t humOffset = settings_.dhtHumOffsetPct();
	if (request->hasParam("dhtTempOffset", true)) {
		tempOffset = static_cast<int16_t>(request->getParam("dhtTempOffset", true)->value().toInt());
	}
	if (request->hasParam("dhtHumOffset", true)) {
		humOffset = static_cast<int16_t>(request->getParam("dhtHumOffset", true)->value().toInt());
	}
	settings_.setDhtOffsets(tempOffset, humOffset);

	settings_.setDhtThresholds(parseOptionalThreshold(request, "dhtTempMin"),
	                            parseOptionalThreshold(request, "dhtTempMax"),
	                            parseOptionalThreshold(request, "dhtHumMin"),
	                            parseOptionalThreshold(request, "dhtHumMax"));
	settings_.setGooglePingMaxLatencyMs(parseOptionalPingMs(request, "googlePingMax"));

	String mode = request->hasParam("mode", true) ? request->getParam("mode", true)->value() : "slide";
	if (mode == "static") {
		size_t idx = 0;
		if (request->hasParam("staticSource", true)) {
			idx = static_cast<size_t>(request->getParam("staticSource", true)->value().toInt());
		}
		if (idx >= kAvailableDataSourceCount) idx = 0;
		settings_.setStaticMode(kAvailableDataSources[idx]);
	} else {
		uint16_t interval = SettingsManager::kSlideIntervalDefaultSec;
		if (request->hasParam("slideInterval", true)) {
			interval = static_cast<uint16_t>(request->getParam("slideInterval", true)->value().toInt());
		}
		settings_.setSlideMode(interval);
	}

	request->redirect("/settings?saved=1");
}

void WebServerManager::handlePingAdd(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("ip", true)) {
		String ip = request->getParam("ip", true)->value();
		if (!ip.isEmpty()) {
			settings_.addPingTarget(ip);
		}
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handlePingRemove(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("i", true)) {
		size_t idx = static_cast<size_t>(request->getParam("i", true)->value().toInt());
		settings_.removePingTarget(idx);
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handleSensormeterAdd(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("ip", true)) {
		String ip = request->getParam("ip", true)->value();
		if (!ip.isEmpty()) {
			settings_.addSensormeterTarget(ip);
		}
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handleSensormeterRemove(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("i", true)) {
		size_t idx = static_cast<size_t>(request->getParam("i", true)->value().toInt());
		settings_.removeSensormeterTarget(idx);
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handleSensormeterThresholds(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("i", true)) {
		size_t idx = static_cast<size_t>(request->getParam("i", true)->value().toInt());
		uint8_t sensorIdx =
		    request->hasParam("s", true) ? static_cast<uint8_t>(request->getParam("s", true)->value().toInt()) : 0;
		// "ip" ist nur in der ersten Sensor-Zeile im Formular enthalten
		// (siehe buildSettingsPage()) - erlaubt, ein Ziel neu zu setzen,
		// ohne es vorher entfernen zu muessen (geht beim letzten
		// verbleibenden Ziel ohnehin nicht, siehe removeSensormeterTarget()).
		if (request->hasParam("ip", true)) {
			String ip = request->getParam("ip", true)->value();
			ip.trim();
			if (!ip.isEmpty()) {
				settings_.setSensormeterTargetIp(idx, ip);
			}
		}
		settings_.setSensormeterThresholds(idx, sensorIdx, parseOptionalThreshold(request, "tempMin"),
		                                    parseOptionalThreshold(request, "tempMax"),
		                                    parseOptionalThreshold(request, "humMin"),
		                                    parseOptionalThreshold(request, "humMax"));
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handlePingThreshold(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("i", true)) {
		size_t idx = static_cast<size_t>(request->getParam("i", true)->value().toInt());
		settings_.setPingMaxLatencyMs(idx, parseOptionalPingMs(request, "maxMs"));
	}
	request->redirect("/settings?saved=1");
}

void WebServerManager::handleNetworkSave(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;

	String ipMode = request->hasParam("ipMode", true) ? request->getParam("ipMode", true)->value() : "dhcp";
	if (ipMode == "static") {
		IPAddress ip, gateway, subnet;
		bool okIp = request->hasParam("ip", true) && ip.fromString(request->getParam("ip", true)->value());
		bool okGw =
		    request->hasParam("gateway", true) && gateway.fromString(request->getParam("gateway", true)->value());
		bool okSn =
		    request->hasParam("subnet", true) && subnet.fromString(request->getParam("subnet", true)->value());
		if (okIp && okGw && okSn) {
			// Kollisions-Check: nur wenn sich die IP gegenueber der aktuell
			// aktiven Adresse tatsaechlich aendert - erlaubt, dieselbe (eigene)
			// IP erneut als statisch zu bestaetigen, ohne sich selbst als
			// "belegt" zu melden.
			if (ip != WiFi.localIP() && ipRespondsToPing(ip)) {
				request->send(409, "text/plain",
				               "IP " + ip.toString() +
				                   " ist bereits belegt (ein Geraet antwortet auf Ping) - Einstellungen wurden "
				                   "NICHT uebernommen.");
				return;
			}
			wlan_.saveStaticIp(ip, gateway, subnet);
		}
	} else {
		// Live-Test auf der bestehenden Verbindung, bevor DHCP tatsaechlich
		// uebernommen wird: WiFi.config() mit Nulladressen erzwingt einen
		// neuen DHCP-Lauf, OHNE die WLAN-Assoziation zu trennen (reiner
		// L3-Vorgang). Erst bei tatsaechlich erhaltener Lease (IP != 0.0.0.0)
		// gilt der Test als erfolgreich - andernfalls bleibt die zuletzt
		// gespeicherte (ggf. statische) Konfiguration unangetastet und es
		// wird KEIN Neustart ausgeloest.
		IPAddress zero(0, 0, 0, 0);
		WiFi.config(zero, zero, zero);
		unsigned long start = millis();
		bool gotLease = false;
		while (millis() - start < kDhcpTestTimeoutMs) {
			if (WiFi.localIP() != zero) {
				gotLease = true;
				break;
			}
			delay(100);
		}
		if (!gotLease) {
			// Vorherige Live-Konfiguration wiederherstellen, falls zuvor eine
			// statische IP aktiv war, damit die laufende Verbindung (inkl.
			// dieser HTTP-Antwort) nicht im DHCP-Test-Zwischenzustand haengen
			// bleibt.
			IPAddress prevIp, prevGw, prevSn;
			if (wlan_.loadStaticIp(prevIp, prevGw, prevSn)) {
				WiFi.config(prevIp, prevGw, prevSn);
			}
			request->send(409, "text/plain",
			               "Kein DHCP-Server im Netz gefunden (keine Lease erhalten) - Einstellungen NICHT "
			               "uebernommen.");
			return;
		}
		wlan_.clearStaticIp();
	}

	request->send(200, "text/plain", "Netzwerkeinstellungen gespeichert, Geraet startet neu ...");
	delay(500);
	ESP.restart();
}

void WebServerManager::handleOtaUploadChunk(AsyncWebServerRequest *request, const String &filename,
                                             size_t index, uint8_t *data, size_t len, bool final) {
	if (!checkAuth(request)) return;
	(void)filename;

	if (index == 0) {
		otaInProgress_ = ota_.beginLocalUpdate(UPDATE_SIZE_UNKNOWN);
		otaSuccess_ = false;
	}
	if (otaInProgress_) {
		otaInProgress_ = ota_.writeLocalUpdateChunk(data, len);
	}
	if (final && otaInProgress_) {
		otaSuccess_ = ota_.endLocalUpdate();
	}
}

void WebServerManager::handleOtaUploadComplete(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (otaSuccess_) {
		request->send(200, "text/plain", "Update erfolgreich, Geraet startet neu ...");
		delay(500);
		ESP.restart();
	} else {
		request->send(500, "text/plain", "Update fehlgeschlagen");
	}
}

void WebServerManager::begin() {
	// "/" ist bewusst NICHT per checkAuth() geschuetzt - oeffentliches
	// Live-Status-Dashboard, siehe docs/entscheidungen.md.
	server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		request->send(200, "text/html", buildDashboardPage());
	});
	server_.on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if (!checkAuth(request)) return;
		request->send(200, "text/html", buildSettingsPage(request->hasParam("saved")));
	});
	server_.on("/settings/export", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if (!checkAuth(request)) return;
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", buildConfigExport());
		response->addHeader("Content-Disposition", "attachment; filename=sensormeter-display-config.txt");
		request->send(response);
	});

	server_.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSave(request); });
	server_.on("/ping/add", HTTP_POST, [this](AsyncWebServerRequest *request) { handlePingAdd(request); });
	server_.on("/ping/remove", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handlePingRemove(request); });
	server_.on("/sensormeter/add", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handleSensormeterAdd(request); });
	server_.on("/sensormeter/remove", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handleSensormeterRemove(request); });
	server_.on("/sensormeter/thresholds", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handleSensormeterThresholds(request); });
	server_.on("/ping/threshold", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handlePingThreshold(request); });
	server_.on("/network/save", HTTP_POST, [this](AsyncWebServerRequest *request) { handleNetworkSave(request); });

	server_.on(
	    "/ota/upload", HTTP_POST, [this](AsyncWebServerRequest *request) { handleOtaUploadComplete(request); },
	    [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len,
	           bool final) { handleOtaUploadChunk(request, filename, index, data, len, final); });

	server_.begin();
}
