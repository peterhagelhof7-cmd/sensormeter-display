#include "WebServerManager.h"

#include <Update.h>

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

} // namespace

WebServerManager::WebServerManager(SettingsManager &settings, BacklightManager &backlight, OtaManager &ota)
    : settings_(settings), backlight_(backlight), ota_(ota) {}

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

String WebServerManager::buildPage() const {
	bool isSlide = settings_.mode() == OperatingMode::Slide;

	String html;
	html.reserve(4000);
	html += "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"UTF-8\">";
	html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
	html += "<title>" + escapeHtml(settings_.deviceName()) + "</title>";
	html += "<style>body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px;}"
	        "fieldset{margin-bottom:16px;}label{display:block;margin:6px 0 2px;}"
	        "input,select{width:100%;box-sizing:border-box;padding:4px;}"
	        "button{margin-top:8px;padding:6px 12px;}"
	        ".row{display:flex;gap:8px;align-items:center;margin:4px 0;}"
	        ".row span{flex:1;}</style></head><body>";

	html += "<h2>" + escapeHtml(settings_.deviceName()) + "</h2>";

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

	html += "<fieldset><legend>Sensormeter-Ziel (SNMP)</legend>";
	html += "<label>IP-Adresse</label><input name=\"smIp\" value=\"" + escapeHtml(settings_.sensormeterIp()) + "\">";
	html += "<label>Community</label><input name=\"smCommunity\" value=\"" +
	        escapeHtml(settings_.sensormeterCommunity()) + "\">";
	html += "</fieldset>";

	html += "<button type=\"submit\">Speichern</button>";
	html += "</form>";

	// --- Ping-Ziele (eigene, dynamische Liste - separate Formulare) ---
	html += "<fieldset><legend>Ping-Ziele (max. " + String(SettingsManager::kMaxPingTargets) + ")</legend>";
	for (size_t i = 0; i < settings_.pingTargetCount(); i++) {
		html += "<div class=\"row\"><span>" + escapeHtml(settings_.pingTargetIp(i)) + "</span>";
		html += "<form method=\"POST\" action=\"/ping/remove\" style=\"margin:0\">";
		html += "<input type=\"hidden\" name=\"i\" value=\"" + String(i) + "\">";
		html += "<button type=\"submit\">Entfernen</button></form></div>";
	}
	if (settings_.pingTargetCount() < SettingsManager::kMaxPingTargets) {
		html += "<form method=\"POST\" action=\"/ping/add\">";
		html += "<label>Neues Ziel (IP-Adresse)</label><input name=\"ip\">";
		html += "<button type=\"submit\">Hinzufuegen</button></form>";
	}
	html += "</fieldset>";

	// --- Firmware-Update ---
	html += "<fieldset><legend>Firmware-Update</legend>";
	html += "<form method=\"POST\" action=\"/ota/upload\" enctype=\"multipart/form-data\">";
	html += "<input type=\"file\" name=\"file\" accept=\".bin\">";
	html += "<button type=\"submit\">.bin hochladen</button></form>";
	html += "<p><a href=\"https://github.com/peterhagelhof7-cmd/sensormeter-display/releases\">"
	        "Releases auf GitHub</a></p>";
	html += "</fieldset>";

	html += "</body></html>";
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
	if (request->hasParam("smIp", true) && request->hasParam("smCommunity", true)) {
		settings_.setSensormeterTarget(request->getParam("smIp", true)->value(),
		                                request->getParam("smCommunity", true)->value());
	}

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

	request->redirect("/");
}

void WebServerManager::handlePingAdd(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("ip", true)) {
		String ip = request->getParam("ip", true)->value();
		if (!ip.isEmpty()) {
			settings_.addPingTarget(ip);
		}
	}
	request->redirect("/");
}

void WebServerManager::handlePingRemove(AsyncWebServerRequest *request) {
	if (!checkAuth(request)) return;
	if (request->hasParam("i", true)) {
		size_t idx = static_cast<size_t>(request->getParam("i", true)->value().toInt());
		settings_.removePingTarget(idx);
	}
	request->redirect("/");
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
	server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if (!checkAuth(request)) return;
		request->send(200, "text/html", buildPage());
	});

	server_.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSave(request); });
	server_.on("/ping/add", HTTP_POST, [this](AsyncWebServerRequest *request) { handlePingAdd(request); });
	server_.on("/ping/remove", HTTP_POST,
	           [this](AsyncWebServerRequest *request) { handlePingRemove(request); });

	server_.on(
	    "/ota/upload", HTTP_POST, [this](AsyncWebServerRequest *request) { handleOtaUploadComplete(request); },
	    [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len,
	           bool final) { handleOtaUploadChunk(request, filename, index, data, len, final); });

	server_.begin();
}
