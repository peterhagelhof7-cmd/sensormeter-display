# Sensormeter Display (HW-458B)

ESP32-basiertes 2,8"-Touchdisplay-System (Board HW-458B: ESP-WROOM-32 +
ST7789P3 TFT, 240x320, resistiver 4-Draht-Touch). Zeigt wahlweise
Innenraumklima (DHT11), Uhrzeit/Datum, Messwerte des
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekts
(SNMP) oder Ping-Laufzeiten an. WLAN-Konfiguration direkt am Gerät per
Touch, keine Cloud-Anbindung.

## Dokumentation

| Datei | Inhalt |
|---|---|
| [docs/lastenheft.txt](docs/lastenheft.txt) | Fachliche Anforderungen: Hardware, GUI, Betriebsarten, Datenquellen, Fehlerbehandlung |
| [docs/pflichtenheft.txt](docs/pflichtenheft.txt) | Technische Umsetzung: Softwaremodule, Datenspeicherung, Startup-Flow, Fehlerbehandlung |
| [docs/implementierungsplan.html](docs/implementierungsplan.html) | Visueller Implementierungsplan P0–P8 (lokal im Browser öffnen) |
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll: Pinbelegung, Touch-Ansteuerung, Schriftwahl |
| [docs/stueckliste.md](docs/stueckliste.md) | Bauteile pro Gerät + Werkzeug |
| [docs/ZABBIX.md](docs/ZABBIX.md) | Zabbix-Integration (agentenlose ICMP-Erreichbarkeitsüberwachung) |
| [docs/zabbix-template-sensormeter-display.yaml](docs/zabbix-template-sensormeter-display.yaml) | Fertiges Zabbix-Template |

`docs/lastenheft.txt` ist die strukturierte Ausarbeitung der ursprünglichen
`Beschreibung.txt` (führendes Anforderungsdokument, liegt außerhalb dieses
Repos in der Materialsammlung). Ältere Lastenheft-Entwürfe
(`Lastenheft_ESP32_Touchsystem.pdf`, `_v2.pdf`) und eine verworfene
Hardware-Alternative (ESP32-S3/Heemol) wurden bewusst **nicht** übernommen.

## Hardware

- Board: HW-458B (ESP-WROOM-32, 2,8" TFT ST7789P3, resistiver 4-Draht-Touch)
- DHT11: Erweiterungsanschluss "IO2" → GPIO22 (nicht literal GPIO2, siehe
  Begründung in `docs/lastenheft.txt` Abschnitt 2)
- RGB-Status-LED: GPIO17/4/16

## Firmware

`firmware/` ist ein PlatformIO-Projekt (Board `esp32dev`, Framework Arduino).
Aktueller Stand: **P8 — Ping-Überwachung** (siehe
[docs/implementierungsplan.html](docs/implementierungsplan.html)). P6 (Snake)
ist als Platzhalter im Menü vorhanden, das Spiel selbst ist optional und noch
offen.

```
cd firmware
pio run              # bauen
pio run --target upload   # flashen
pio device monitor   # seriellen Log ansehen (115200 Baud)
```

Enthalten (P0–P5, P7–P8):
- TFT_eSPI-Ansteuerung des ST7789P3 (Querbetrieb, 320x240)
- Touch-Bit-Bang-Treiber (`TouchManager`) inkl. 2-Punkt-Kalibrierung,
  Kalibrierdaten in NVS (Preferences)
- WLAN-Ersteinrichtung (`WlanManager`, `WifiOnboarding`): Scan mit
  "Aktualisieren"-Button, Netzliste mit Empfangsbalken, Bildschirmtastatur
  (Buchstaben + Sonderzeichen) zur PSK-Eingabe, Speicherung in NVS,
  automatischer Verbindungsaufbau bei jedem weiteren Start
- Statusleiste (`StatusBar`): Zahnrad (antippbar, öffnet Einstellungen),
  WLAN-Empfangsbalken (blinkt ohne Verbindung), DHT11-Werte oben;
  Uhrzeit/Datum unten (entfällt im Static-Modus mit Quelle "Uhrzeit");
  wechselt bildschirmweit auf roten Hintergrund bei anhaltendem Ping-Fehler
- NTP-Zeit (`TimeSync`, vorgezogen aus P4): de.pool.ntp.org, deutsche
  Zeitzone inkl. Sommerzeit
- DHT11-Datenquelle (`SensorManager`, `GraphManager`): Abfrage alle 5s mit
  Plausibilitätsprüfung, Verlaufsgraph (24 Messpunkte/30min = 12h,
  Temperatur rot/Luftfeuchte blau) mit Ringpuffer-Persistenz in
  `history.csv` auf LittleFS
- Uhrzeit-Datenquelle (`ClockView`): HH:MM groß (oberes zwei Drittel),
  Wochentag + Datum (unteres Drittel)
- Sensormeter-Datenquelle (`SensormeterClient`, `SensormeterView`): schlanker,
  selbstgeschriebener SNMP-v1-GET-Client (siehe Entscheidungsprotokoll),
  Abfrage alle 30s, Temperatur/Luftfeuchte des Sensormeter-Projekts
- Ping-Datenquellen (`PingManager`, `PingView`): Durchschnitt der letzten
  5 Latenzen zu google.com; bis zu 5 zusätzliche Ziele als eigene Ansicht
  (IP + Status, grün/rot); LED blinkt rot bei >1 Min. Ausfall (`LedManager`)
- Einstellungen (`SettingsUI`, `SettingsManager`, `BacklightManager`):
  Slide (Intervall 5–60s), Static (Quellenauswahl aus allen 5 Datenquellen),
  Snake (Platzhalter, Spiel folgt optional in P6), Systemeinstellungen
  (Helligkeit ±, WLAN neu wählen, Sensormeter-Ziel, Ping-Ziele über
  `NumericKeypad`) — alles in NVS persistiert

Noch nicht verifiziert: reale Hardware (TFT-Pinbelegung über ein passendes
Referenzdesign erschlossen, nicht am eigenen Board nachgemessen — siehe
`docs/entscheidungen.md`).

**Auf einem anderen/frischen Windows-PC**: [`scripts/flash-sensormeter-display.ps1`](scripts/flash-sensormeter-display.ps1)
(oder `.cmd` zum Doppelklicken) richtet Python/Git/PlatformIO automatisch
ein, klont dieses Repo falls nötig und baut/flasht das per USB angeschlossene
Board in einem Rutsch. Details: [`scripts/README.md`](scripts/README.md).

## Zusammenhang mit dem Sensormeter-Projekt

Die Datenquelle "Sensormeter" fragt das separate
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekt
(WT32-ETH01) per SNMP v1 ab. Die OID-Struktur ist dort bereits vollständig
dokumentiert und implementiert (`.1.3.6.1.4.1.99999.x`) und wird 1:1
übernommen (siehe `docs/lastenheft.txt` Abschnitt 7.3).

## Über dieses Projekt

Repo-Struktur und Dokumentation entstehen in Zusammenarbeit mit
[Claude](https://claude.com/claude-code) (Anthropic) als KI-Coding-Assistent.
