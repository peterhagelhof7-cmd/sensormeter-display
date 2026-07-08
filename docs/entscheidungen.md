# Entscheidungsprotokoll — Sensormeter Display

## Webserver + OTA (Zusatzfunktion nach P0-P8)

### Async statt synchron (ESPAsyncWebServer/AsyncTCP)
Wie im Sensormeter-Projekt: async, damit HTTP-Anfragen den Hauptloop
(Touch, DHT11/Sensormeter/Ping-Polling, Snake) nicht blockieren. Dieselben,
dort bereits bewaehrten Bibliotheksversionen uebernommen
(`esp32async/ESPAsyncWebServer@^3.11.2`, `esp32async/AsyncTCP@^3.4.10`).

### Kein REST-API/JSON, kein HTTPS-Client
Anders als das Sensormeter-Projekt (dort: Dashboard + REST-API) geht es
hier nur um ein Einstellungsformular - ArduinoJson wurde daher bewusst
NICHT hinzugefuegt (spart Flash, keine funktionale Notwendigkeit). Ebenso
kein HTTPS-Client fuer einen Remote-Versionscheck (hätte laut Sensormeter-
Projekt allein ca. 168 KB gekostet) - Firmware-Update nur per lokalem
`.bin`-Upload, dafuer ein Link zu den GitHub-Releases zum manuellen
Herunterladen (identisches Vorgehen wie dort).

### Custom-Partitionstabelle fuer OTA (partitions_ota.csv) - Korrektur, war unnoetig

**Korrektur (nachtraeglich, beim Entwurf des Sensormeter-WLAN-Projekts
aufgefallen):** Die Aussage unten, das PlatformIO-Standardschema habe nur
eine einzelne App-Partition, war **falsch** und wurde nie tatsaechlich
gegen den unveraenderten Standard geprueft - nur angenommen. Ein
tatsaechlicher Build des Sensormeter-Projekts (WT32-ETH01, identisches
`board = esp32dev`, keine eigene `partitions.csv`) zeigt per
`gen_esp32part.py`, dass das Standardschema fuer dieses Board bereits
`ota_0`/`ota_1` (je 1280K) mitbringt, dazu `spiffs`/LittleFS (1408K) und
eine `coredump`-Partition (64K) - OTA haette also auch ohne eigene Tabelle
funktioniert.

Die eigene `partitions_ota.csv` (nvs 20K, otadata 8K, app0/app1 je 1280K,
spiffs 1472K) bleibt trotzdem bestehen, da sie funktional gleichwertig ist
und bereits produktiv verifiziert wurde (siehe unten) - der einzige
tatsaechliche Unterschied zum Standard ist der Verzicht auf die
64K-Coredump-Partition zugunsten von 64K mehr LittleFS, was fuer die
24-Zeilen-`history.csv` ohnehin irrelevant ist. Eine Ruecknahme auf den
Standard wuerde keinen Vorteil bringen und nur unnoetiges Risiko fuer
bereits geflashte Geraete schaffen.

Urspruengliche (nicht mehr zutreffende) Begruendung, zur Nachvollziehbarkeit
belassen: "OTA braucht zwei App-Slots (ota_0/ota_1) statt der einen
App-Partition im PlatformIO-Standardschema." Eigene Tabelle auf dem
4-MB-Flash (32 Mbit laut Datenblatt): nvs 20K, otadata 8K, app0 1280K,
app1 1280K, spiffs (LittleFS) 1472K. app0/app1 sind genauso gross wie die
bisherige einzelne App-Partition (aktuelle Firmware liegt bei ~73%,
~350 KB Reserve je Slot). Verifiziert mit `gen_esp32part.py` gegen die
tatsaechlich generierte `partitions.bin` - Ausgabe zeigt exakt die
erwarteten ota_0/ota_1/spiffs-Eintraege (nur die falsche Praemisse, dass
dies noetig sei, wurde nicht hinterfragt).

**Wichtig fuer spaetere Firmware-Updates:** Ein Geraet, das bereits mit der
alten (Single-App-)Partitionstabelle geflasht wurde, braucht vor dem ersten
Flash dieser Version einen vollstaendigen Flash-Loeschvorgang
(`pio run --target erase` bzw. `esptool.py erase_flash`), da sich NVS-/
App-Offsets verschoben haben. Bisher ist noch kein reales Geraet mit einer
Vorversion geflasht worden, betrifft also aktuell niemanden praktisch.

### Bug gefunden und behoben: Datenrennen auf SettingsManager (fehlender Mutex)
ESPAsyncWebServer verarbeitet Anfragen in einem eigenen FreeRTOS-Task, NICHT
im Arduino-Hauptloop. `SettingsManager` wird aber sowohl vom Hauptloop/
Touch-UI als auch von den Web-Handlern gelesen und geschrieben (z. B.
Sensormeter-Ziel, Ping-Ziele) - ohne Sperre waere das ein Datenrennen auf
den `String`-Feldern (Arduino-Strings sind nicht thread-sicher), potenziell
mit Heap-Korruption oder Absturz. Behoben nach demselben Muster wie
`DataManager` im Sensormeter-Projekt: ein `SemaphoreHandle_t`-Mutex,
Getter/Setter nehmen ihn vor jedem Zugriff. `BacklightManager` (einfacher
LEDC-Registerzugriff) und `OtaManager` (nur waehrend eines einzelnen
Uploads genutzt) wurden bewusst NICHT zusaetzlich abgesichert - das
Risiko/der Nutzen einer Sperre steht dort in keinem Verhaeltnis zum
Aufwand.

### Web-Passwort statt fester Zugangsdaten, aber kein HTTPS
HTTP-Basic-Auth (Benutzername fest "admin", Passwort einstellbar, Default
"admin") wie im Sensormeter-Projekt. Das Passwort wird im Formular selbst
im Klartext vorausgefuellt (`value="..."`) - identische Kompromiss-
Entscheidung wie beim PSK-Eingabefeld in der WLAN-Ersteinrichtung (P1):
einfache Kontrolle wichtiger als Sichtschutz auf einem lokalen
Konfigurationsgeraet, zumal ein Angreifer mit Zugriff auf die
authentifizierte Seite das Passwort ohnehin schon kennt/kennen muss.

### Ressourcenverbrauch: +63 KB Flash, +400 Byte RAM
Vorab abgeschaetzt (Analogie zum Sensormeter-Projekt): ca. 97 KB allein fuer
ESPAsyncWebServer/AsyncTCP/ArduinoJson. Tatsaechlich (ohne ArduinoJson,
siehe oben): Flash 73,3% (960.453/1.310.720 Byte je App-Slot), RAM 14,4%
(47.240/327.680 Byte) - deutlich guenstiger als angenommen.

## P6 — Snake (optional)

### Touch-Drittel-Ueberschneidung: vertikale Zonen haben Prioritaet
`lastenheft.txt` Abschnitt 6.3 beschreibt vier Touch-Zonen (oberes/unteres/
linkes/rechtes Drittel), legt aber nicht fest, wie die Ecken aufgeloest
werden, wo sich z. B. "oberes Drittel" und "linkes Drittel" ueberlappen.
`SnakeGame::zoneForTouch()` prueft zuerst oben/unten, dann links/rechts -
ein Tipp in der oberen linken Ecke zaehlt also als "hoch". Einfachste,
deterministische Aufloesung ohne zusaetzliche UI-Elemente.

### Raster 16px-Zellen, 20x13 Felder, Kopfzeile fuer Punktestand
Bildschirm 320x240, 24px oben fuer die Punkteanzeige reserviert, Rest
(216px) in 16px-Zellen aufgeteilt -> 13 Reihen (208px genutzt, 8px Rand
unten). 16px-Zellen sind auf dem 2,8"-Display gut antippbar/erkennbar und
ergeben ein handhabbares Feld (260 Zellen max. Schlangenlaenge).

### Nicht-blockierender Spiel-Loop (Touch-Poll + fester Tick nebeneinander)
Anders als alle bisherigen Screens (Onboarding, Einstellungen, Tastenfeld),
die auf einen vollstaendigen Tipp-Zyklus warten (`waitForTapEvent`), braucht
Snake einen laufenden Takt unabhaengig vom Touch. `SnakeGame::run()` fragt
Touch alle ~15ms ab (aktualisiert nur die gewuenschte Richtung) und bewegt
die Schlange unabhaengig davon alle 200ms (`kTickIntervalMs`) - Muster
uebernommen aus dem bereits etablierten Hauptloop (`main.cpp`), nicht neu
erfunden.

### Highscore in eigenem NVS-Namespace statt ueber SettingsManager
`SnakeGame` verwaltet seinen Highscore direkt ueber einen eigenen
`Preferences`-Namespace ("snake") statt den Umweg ueber `SettingsManager`
zu nehmen - konsistent damit, wie auch `TouchManager` (Kalibrierung) und
`WlanManager` (Zugangsdaten) jeweils ihre eigene NVS-Namespace verwalten,
statt alle Einstellungen zentral zu buendeln.

### Ressourcenverbrauch geringer als geschaetzt
Vor der Umsetzung geschaetzt: +10-20 KB Flash. Tatsaechlich: **+2,4 KB
Flash, +136 Byte RAM** (896.165/1.310.720 Flash, 46.840/327.680 RAM) - da
Snake keine neue Bibliothek braucht und ausschliesslich bereits gelinkte
Bausteine (TFT_eSPI-Primitiven, Preferences, `random()`) wiederverwendet.

## P7/P8 — Sensormeter-Anbindung (SNMP), Ping-Überwachung

### OID-Kodierung im Sensormeter-Projekt verifiziert statt angenommen
`lastenheft.txt` Abschnitt 7.3 nennt die OID-Struktur, aber nicht die
Kodierung. Im Quellcode des Sensormeter-Projekts nachgesehen
(`SNMPManager.cpp`): Temperatur/Luftfeuchte sind als `INTEGER x10`
kodiert (z. B. 235 = 23,5 °C), nicht als Float oder Integer ohne
Skalierung. `SensormeterClient` teilt entsprechend durch 10.

### Eigener, minimaler SNMP-v1-GET-Client statt Bibliothek
Es existiert keine gaengige Arduino/PlatformIO-Bibliothek für einen
SNMP-**Client** (nur für SNMP-**Agenten**, wie die im Sensormeter-Projekt
selbst verwendete). Da nur zwei feste, bekannte skalare Integer-OIDs alle
30s abgefragt werden, wurde ein sehr schlanker, hart auf diesen Anwendungsfall
zugeschnittener BER-Encoder/Decoder geschrieben (`SnmpClient`) statt eine
vollstaendige SNMP-Implementierung zu bauen oder zu vendorn. Einschraenkungen:
nur Short-Form-Laengen (Pakete hier immer klein genug), keine explizite
Auswertung von `errorStatus` (siehe Kommentar in `SnmpClient.cpp`) - im
Fehlerfall (falsche OID) koennte theoretisch `errorIndex` faelschlich als
Wert interpretiert werden. Unkritisch, da die OIDs hart einprogrammiert und
bekannt korrekt sind; realistisches Fehlerbild ist "keine Antwort"
(Geraet nicht erreichbar), nicht "falsche OID".

### ESP32Ping als Bibliothek statt eigenem ICMP-Code
Anders als bei SNMP wurde fuer den Ping-Client (P8) eine etablierte
Bibliothek (`marian-craciunescu/ESP32Ping`) verwendet statt eigenem
ICMP-Code: Rohes ICMP erfordert tieferen lwIP-Zugriff, den die Bibliothek
bereits robust kapselt - hier lohnt sich das Vendoring/Selbstschreiben
nicht wie bei SNMP (dort ist das Protokoll trivial und die Bibliothekslage
duenn).

### Ping-Zyklus entzerrt (max. 1 Google-Ping + 1 Ziel-Ping pro update())
Ein Ping-Versuch kann bei Nichterreichbarkeit bis zu ~1s blockieren. Bei
bis zu 5 zusaetzlichen Zielen haette ein "alle auf einmal"-Ansatz den
Hauptloop und damit die Touch-Reaktion um mehrere Sekunden blockieren
koennen. Stattdessen: pro `update()`-Aufruf (alle 2s) genau ein
Google-Ping UND round-robin genau ein Zusatzziel - begrenzt die
Blockierzeit pro Aufruf auf ca. 2 Ping-Timeouts.

### Alarmzustand (rote Anzeige) als bgColor-Parameter durchgereicht
"Ändere die Display-Farbe in rot" (lastenheft.txt Abschnitt 9) gilt
bildschirmweit, unabhaengig von der aktuell aktiven Datenquelle. Statt
eines globalen Zustands, den jede View selbst abfragt, wird die
Hintergrundfarbe als expliziter Parameter an `GraphManager::drawFullScreen`,
`ClockView::draw`, `SensormeterView::draw`, `PingView::draw*` und
`StatusBar::draw` durchgereicht - haelt die Views unabhaengig testbar und
macht die Abhaengigkeit im Funktionssignatur sichtbar. Textfarbe bleibt
Schwarz (nicht explizit gefordert, Kontrast auf Rot bleibt ausreichend).

### RGB-LED-Polaritaet angenommen (HIGH = an)
Das Datenblatt spezifiziert nicht, ob die RGB-LED gemeinsame Anode oder
Kathode hat. `LedManager` nimmt HIGH=an (gemeinsame Kathode) an - am
eigenen Board zu verifizieren; falls falsch, ist nur die Polaritaet in
`LedManager::setColor` zu invertieren.

### Sensormeter-Ziel und Ping-Ziele in Systemeinstellungen platziert
`lastenheft.txt` beschreibt nicht, ueber welchen Bildschirm die
Sensormeter-IP oder die Ping-Ziel-Liste eingegeben werden. Da
Systemeinstellungen bereits WLAN-Neukonfiguration (ebenfalls
netzwerkbezogen) enthaelt, wurden beide dort als zusaetzliche
Menuepunkte ergaenzt statt einen weiteren Menue-Bereich einzufuehren.

### Numerisches Tastenfeld statt Bildschirmtastatur fuer IP-Eingaben
IP-Adressen brauchen nur Ziffern und Punkt. Die vorhandene
Bildschirmtastatur (`WifiOnboarding`, P1) ist auf PSK-Eingabe zugeschnitten
(Buchstaben+Sonderzeichen) und unnoetig komplex fuer diesen Zweck - stattdessen
ein eigenes, einfaches Telefon-Tastenfeld (`NumericKeypad`), wiederverwendet
fuer Sensormeter-Ziel UND Ping-Ziele.

### Bug gefunden und behoben: Static-Quellenliste lief ueber den Bildschirmrand hinaus
Die Datenquellenliste in den Static-Einstellungen ist mit P7/P8 von 2 auf
5 Eintraege gewachsen, nutzte aber weiterhin die fuer das 4-Punkt-Hauptmenu
bemessene Zeilenhoehe (44px) - der 5. Eintrag (Ping-Ziele) waere teilweise
unterhalb von y=240 gelandet und damit weder sichtbar noch antippbar.
Behoben mit eigener, kompakterer Zeilenhoehe (40px) nur fuer diese Liste;
zusaetzlich die Datenquellen-Labels gekuerzt ("Innentemperatur (DHT11)" u. ae.
waeren in der schmalen Zeile ohnehin abgeschnitten worden).

## P4/P5 — Uhrzeit-Datenquelle, Einstellungen/Betriebsarten

### LEDC-API der installierten Arduino-ESP32-Version ist die alte, kanalbasierte
`ledcAttach(pin, freq, bits)` (neue, pin-basierte API aus Arduino-ESP32 3.x)
existiert in der hier per PlatformIO installierten Framework-Version nicht -
der Compiler schlug `ledcAttachPin` als Alternative vor. `BacklightManager`
nutzt daher die aeltere Kanal-API (`ledcSetup` + `ledcAttachPin` + `ledcWrite(channel,...)`,
Kanal 4 gewaehlt, da 0-3 oft von anderen Peripherien/Beispielen belegt sind).

### Nur DHT11 und Uhrzeit in der Static-Quellenauswahl
`DataSource` (siehe `DataSource.h`) enthaelt aktuell nur die beiden bereits
implementierten Quellen. Sensormeter (P7) und Ping (P8) werden dort ergaenzt,
sobald sie existieren - die Auswahlliste in `SettingsUI` iteriert bereits
generisch ueber `kAvailableDataSources`, erfordert also keine strukturelle
Aenderung, nur einen weiteren Eintrag.

### Statusleisten-Redraw wird nach dem Schliessen der Einstellungen erzwungen
Der Einstellungen-Dialog zeichnet vollflaechig ueber die normale Anzeige.
Nach dessen Rueckkehr werden `contentDirty=true` und `lastStatusBarMs=0`
gesetzt, damit im naechsten `loop()`-Durchlauf sofort alles neu gezeichnet
wird statt bis zum naechsten regulaeren Timer zu warten (bis zu 5s
Uhrzeit-Redraw-Intervall waeren sonst als eingefrorener Bildschirm
wahrgenommen worden).

### Snake (P6) als Platzhalter in der Modus-Liste
Der Menuepunkt "Snake" ist bereits Teil des Einstellungen-Dialogs (wie im
Lastenheft verlangt: Slide/Static/Snake/Systemeinstellungen sind EIN
gemeinsames Auswahlmenu), zeigt aber nur einen Hinweistext und kehrt zurueck,
da das eigentliche Spiel erst P6 ist. Vermeidet eine spaetere Restrukturierung
des Menues.

### Gemeinsame Touch-UI-Bausteine ausgelagert (UiHelpers)
`hitRect`/`waitForTapEvent` gab es zunaechst nur lokal in
`WifiOnboarding.cpp` (P1). Mit `SettingsUI` als zweitem Verbraucher wurden
sie nach `UiHelpers.h/.cpp` verschoben, um Code-Duplikation zu vermeiden -
bewusst nicht schon in P1 vorgezogen, da zu dem Zeitpunkt noch kein zweiter
Verbraucher absehbar war.

## P2/P3 — Statusleiste, DHT11-Datenquelle

### NTP-Sync vorgezogen aus P4
Die permanente Statusleiste (P2) zeigt laut Lastenheft auch Uhrzeit/Datum
an - ohne funktionierende Zeit waere dieses Element sinnlos. Daher wurde
ein schlankes `TimeSync`-Modul (NTP via `de.pool.ntp.org`, deutsche
Zeitzone inkl. Sommerzeit ueber POSIX-TZ-String `CET-1CEST,M3.5.0,M10.5.0/3`)
bereits jetzt ergaenzt statt komplett auf P4 zu warten. Der volle
`TimeManager` in P4 (grosse Uhrzeit-Datenquellen-Ansicht) baut auf diesen
Funktionen auf, ersetzt sie nicht.

### Statusleisten-Layout: zwei Leisten a 36px (11,25% der langen Kante)
Bildschirmaufteilung zentral in `Layout.h`: Statusleiste oben (Zahnrad,
WLAN-Balken, DHT11-Werte) und unten (Uhrzeit/Datum), je 36px von 320px
langer Kante = 11,25%, haelt die 15%-Vorgabe aus `lastenheft.txt` Abschnitt 4
mit Reserve ein. Der verbleibende Inhaltsbereich (168px) zeigt die aktive
Datenquelle - fuer P2/P3 fest die DHT11-Ansicht, Umschaltbarkeit folgt mit
Slide/Static in P5.

### Zahnrad-Symbol vorerst nicht antippbar
Das Zahnrad wird gezeichnet, reagiert aber noch nicht auf Touch - der
Einstellungen-Dialog selbst ist erst P5. Antippen aller Statusleisten-Symbole
wird zusammen mit der Einstellungen-Funktion verdrahtet, um Touch-Zonen
nicht doppelt zu definieren.

### Bug gefunden und behoben: WLAN-Blink-Symbol haette bei fester 1s-Redrawrate nie geblinkt
Erster Entwurf redrawte die gesamte UI im festen 1000ms-Takt; das
WLAN-Blinksymbol schaltet alle 500ms um. 1000ms ist ein exaktes Vielfaches
von 500ms, d. h. bei jedem Redraw waere immer dieselbe Blink-Phase
abgetastet worden (Aliasing) - das Symbol haette real nie sichtbar
geblinkt. Behoben, indem die Statusleiste unabhaengig vom Inhaltsbereich
alle 300ms neu gezeichnet wird (kein ganzzahliges Vielfaches von 500ms).
Der Inhaltsbereich (Graph/Werte) wird zusaetzlich nur bei tatsaechlich
neuer DHT11-Messung neu gezeichnet statt starr im Sekundentakt, spart
unnoetige Redraws.

### Graph ohne Gitternetz, nur Min/Max-Achsenbeschriftung
Fuer 24 Punkte auf 168px Inhaltshoehe wurde bewusst auf ein volles
Gitternetz mit Zwischenwerten verzichtet - zwei Polylinien (Temperatur rot,
Luftfeuchte blau) plus Min/Max-Werte an den Achsenenden reichen fuer die
geforderte Aufloesung (volle °C/%) und sparen Bildschirmplatz auf einem
kleinen Display.

### Ringpuffer-Persistenz: komplette Datei bei jedem neuen Eintrag neu schreiben
`history.csv` hat maximal 24 Zeilen und wird hoechstens alle 30 Minuten neu
geschrieben - Flash-Verschleiss ist bei dieser Frequenz irrelevant, ein
komplexeres Append-Only-Format mit Offset-Verwaltung haette sich nicht
gelohnt.

## P1 — WLAN-Ersteinrichtung

### Bildschirmtastatur: gewichtsbasiertes Zeilenlayout, keine PSK-Maskierung
Die Tastatur (`WifiOnboarding.cpp`) berechnet Tastenbreiten aus relativen
Gewichten je Zeile (z. B. Leertaste = 4x Normalbreite) statt fester
Pixelkoordinaten — vermeidet manuelle Off-by-one-Fehler bei Anpassungen.
Zwei Seiten: Buchstaben (mit Umschalt/CAPS) und Sonderzeichen (SYM/ABC zum
Umschalten), Zeichensatz auf gaengige WPA2-taugliche Sonderzeichen begrenzt.

Die eingegebene PSK wird im Klartext angezeigt, nicht maskiert: Auf einem
Geraet ohne physische Tastatur ist die Kontrolle der Eingabe wichtiger als
Sichtschutz, zumal es sich um ein lokales Konfigurationsgeraet handelt.

### Nur ein gespeichertes WLAN
Es wird genau ein Netz (SSID+PSK) in NVS gespeichert, kein Multi-WLAN mit
Prioritaetsliste. Deckt sich mit der fruehreren Entscheidung, die
Mehr-Netzwerk-/Timeout-/Retry-Details der Vorgaenger-Lastenhefte bewusst
nicht zu uebernehmen (siehe `lastenheft.txt` Abschnitt 11). "WLAN neu
waehlen" in den Systemeinstellungen ueberschreibt die gespeicherten Daten.

### Netzliste ohne Scroll-Funktion
`WlanManager::scan()` liefert maximal 6 Netze (nach Signalstaerke sortiert,
SSID-Duplikate zusammengefasst) — mehr passt ohnehin nicht auf den 240px
hohen Bildschirm ohne Scrollen. Weitere Netze werden nicht angezeigt; bei
Bedarf spaeter durch Scroll-Geste erweiterbar.

### Fehlgeschlagene Verbindung: zurueck zur gecachten Liste
Nach einem gescheiterten Verbindungsversuch wird die zuvor gescannte
Netzliste erneut angezeigt statt automatisch neu zu scannen (WLAN-Scan
dauert mehrere Sekunden) — ein manueller "Aktualisieren"-Tap scannt bei
Bedarf neu.

## P0 — Display- und Touch-Grundgerüst

### TFT-Pinbelegung nicht im Datenblatt enthalten
`CBAA0055-008_DE.pdf` (Hersteller-Datenblatt) listet nur die Touch-Pins
(TP_CLK/CS/DIN/OUT/IRQ) sowie den Backlight-Pin (IO21), aber keine eigenen
SPI-Pins für das ST7789P3-TFT selbst (MOSI/MISO/SCLK/CS/DC/RST). Die vier
GPIOs 12/13/14/15 tauchen im gesamten Pin-Diagramm nirgends auf — ein
starker Hinweis, dass sie intern für das TFT reserviert sind.

Gefunden über das exakt identische Referenzdesign LCDWIKI E32R28T/E32N28T
(`2.8inch_ESP32-32E-7789`): dessen Touch-Pinbelegung (IO25/32/33/36/39) und
Backlight-Pin (IO21) stimmen 1:1 mit unserem Datenblatt überein — hohes
Vertrauen, dass auch die TFT-Pins identisch sind:

```
TFT_MOSI = 13   TFT_MISO = 12   TFT_SCLK = 14
TFT_CS   = 15   TFT_DC   = 2    TFT_RST  = -1 (an EN, kein eigener Pin)
TFT_BL   = 21
```

**Wichtiger Nebenfund:** GPIO2 ist damit die TFT-DC-Leitung (Data/Command)
des Displays — bestätigt die frühere Entscheidung, DHT11 nicht auf
literal GPIO2 zu legen (siehe `lastenheft.txt` Abschnitt 2), sondern auf
GPIO22 über den Steckverbinder "Erweiterungsanschluss IO2". GPIO2 wäre
ohnehin belegt gewesen, unabhängig vom Boot-Strapping-Argument.

**Noch offen:** Diese Pinbelegung ist nicht am eigenen Board nachgemessen,
sondern über ein passendes Referenzdesign erschlossen. Beim ersten
Flash-Versuch auf echter Hardware verifizieren (funktioniert das Display
nicht, zuerst TFT_CS/DC/SCLK/MOSI in `platformio.ini` gegeneinander
tauschen).

### Touch-Ansteuerung: Bit-Bang statt XPT2046-Bibliothek
Verfügbare XPT2046-Arduino-Bibliotheken (u. a. `paulstoffregen/XPT2046_Touchscreen`,
in PlatformIO nur als alte Alpha-Version verfügbar) nutzen intern den
globalen Arduino-`SPI`-Bus. Der ist hier aber bereits vom TFT belegt
(eigene Pinbelegung, siehe oben), und die verfügbare Alpha-Version bietet
keine Möglichkeit, einen alternativen `SPIClass`-Bus zu übergeben.

Statt einen zweiten Hardware-SPI-Bus zu konfigurieren (unnötiger Aufwand
für ein einfaches Protokoll), wird der XPT2046-artige Touch-Controller
direkt per Bit-Bang auf den TP_-Pins angesteuert (`TouchManager.cpp`):
Kommandobyte MSB-first raus, 12-Bit-Ergebnis rein, `TP_IRQ` zeigt per
Low-Pegel eine aktive Berührung an. Timing ist Standard-XPT2046, aber noch
nicht an echter Hardware verifiziert.

### Kalibrierung: 2-Punkt, gespeichert in NVS (Preferences)
Resistive Touchscreens sind in der Regel linear, daher genügt eine
Zwei-Punkt-Kalibrierung (oben-links, unten-rechts antippen) statt einer
aufwendigeren 4- oder 9-Punkt-Prozedur. Speicherung über die ESP32-eigene
`Preferences`-Bibliothek (NVS-Partition), Namespace `touch` — kein eigenes
Dateiformat nötig für vier Ganzzahlen plus ein Flag.

### Schriftart: Font 7 (7-Segment) für Zahlen, Font 4 für Text
`lastenheft.txt` fordert eine systemweite Schriftart "ähnlich einer
7-Segment-Anzeige". TFT_eSPI bringt mit Font 7 einen eingebauten
7-Segment-Stil-Font mit, der aber nur Ziffern, Doppelpunkt, Punkt und Minus
darstellen kann — keine Buchstaben. Für Zahlenwerte (Temperatur, Uhrzeit)
wird Font 7 verwendet; für Text (WLAN-Liste, Bildschirmtastatur, Wochentag)
ein lesbarer Standard-Font (Font 4), da ein reiner 7-Segment-Font dafür
technisch nicht ausreicht.
