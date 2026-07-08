# Entscheidungsprotokoll — Sensormeter Display

## Mehrfach-Sensormeter-Ziele, Geraete-Info-Dialog, Static-IP (Erweiterung ueber lastenheft.txt 7.3 hinaus)

Nutzerwunsch, ueber das urspruengliche Lastenheft hinaus: statt genau eines
Sensormeter-Ziels sollen bis zu 5 (mindestens 1) abfragbar sein, je mit
automatisch generiertem Slide; zusaetzlich ein Info-Dialog fuers Display
selbst (Systemname/IP/DHCP-Static) und echte Static-IP-Konfiguration. Noch
NICHT an echter Hardware verifiziert (kein Sensormeter-Geraet zum Testen
erreichbar) - nur Build+Boot am Geraet bestaetigt. Bei Gelegenheit mit
echtem Sensormeter nachpruefen.

### PRO-Erkennung ueber den Typ-String, nicht ueber eine eigene Konfiguration
Das Sensormeter-Projekt leitet `systemType` automatisch aus `sensor2Enabled`
ab (`deriveSystemType()` in dessen `ConfigManager.cpp`: `"Sensormeter PRO"`
vs. `"Sensormeter"`) - kein separates Konfigurationsfeld. Die Display-Seite
prueft daher einfach auf den exakten String `"Sensormeter PRO"` (OID
`.1.3.0`) statt eine eigene Heuristik zu bauen. Sensor-2-Temperatur/-Feuchte
werden nur abgefragt, wenn dieser Typ erkannt wurde - beim normalen Typ
liefert das Sensormeter-Geraet dafuer ohnehin durchgehend `0` (kein Fehler),
das waere sonst nicht von einer echten Nullmessung unterscheidbar gewesen.

### SnmpClient um getString() erweitert (Refactor auf gemeinsamen Kern)
`SnmpClient` konnte bisher nur INTEGER-Werte lesen (Temperatur/Feuchte).
Fuer Systemname/-typ/Sensor-Namen (SNMP OCTET STRING) wurde die BER-
Response-Verarbeitung in eine gemeinsame private `getRaw()`-Methode
extrahiert (liefert Tag-gefiltert die Rohbytes des letzten passenden
TLV-Elements), `getInteger()`/`getString()` interpretieren die Bytes nur
noch unterschiedlich. Vermeidet Duplikation der Request-Kodierung/
Response-Iteration zwischen beiden Methoden.

### SensormeterManager: Identitaet einmalig pro Ziel, danach nur Messwerte
Pro Ziel wird zuerst Systemname+Typ+Sensor-Name(n) aufgeloest (mehrere
sequentielle SNMP-GETs in einem Rutsch - passiert nur einmal beim
Hinzufuegen/Aendern eines Ziels, ein laengerer Blockierzeitraum bei einem
gerade neu eingetragenen, noch nicht erreichbaren Ziel ist dafuer
akzeptabel, der Nutzer sitzt ohnehin in den Einstellungen). Danach werden
nur noch die eigentlichen Messwerte im Round-Robin aktualisiert (ein Ziel
pro `update()`-Aufruf, `kCheckIntervalMs=4000`) - analog zu
`PingManager::pingNextTarget()`, damit ein nicht erreichbares Ziel nicht
den Hauptloop blockiert.

### SensormeterView rotiert intern durch alle Sensor-Slides
Da "Sensormeter" im Static-Modus weiterhin ein einzelner Menuepunkt bleibt
(Nutzerentscheidung, keine 10 einzelnen Static-Eintraege), aber bis zu 10
Sensor-Slides (5 Ziele x 2 Sensoren bei PRO) anzeigen koennen muss, verwaltet
`SensormeterView` selbst einen Rotationsindex + eigenen `millis()`-Timer
(`kSubSlideIntervalMs=4000`) - unabhaengig davon, wie oft `draw()` vom
Hauptloop aufgerufen wird (gleiches Prinzip wie das WLAN-Blinksymbol in
StatusBar). `main.cpp` behandelt `DataSource::Sensormeter` deshalb wie
`Uhrzeit` als Quelle, die auch ohne neue Messung periodisch neu gezeichnet
werden muss (`periodicDue`), damit die interne Rotation ueberhaupt sichtbar
wird.

### Community bleibt touch-uneditierbar, nur ueber Web
Bestehende Entscheidung uebernommen: das Zifferntastenfeld deckt keine
Buchstaben ab, eine volle Bildschirmtastatur waere fuer dieses eine, selten
geaenderte, gemeinsame Feld unverhaeltnismaessig. Touch-UI zeigt die
Community nur lesend an, Aendern nur ueber das Webinterface.

### Mindestens 1 Sensormeter-Ziel: in SettingsManager statt nur in der UI erzwungen
`removeSensormeterTarget()` verweigert das Entfernen, wenn nur noch ein
Ziel uebrig ist - zentral in `SettingsManager` statt getrennt in Touch- und
Web-UI, damit beide Oberflaechen nicht unabhaengig voneinander auf diese
Regel achten muessen. Ein frisch geflashtes Geraet startet trotzdem bei 0
Zielen (wie bei den Ping-Zielen) - die Regel greift erst, sobald das erste
Ziel hinzugefuegt wurde.

### UiHelpers::drawCloseButton() extrahiert (zweiter Verbraucher: InfoUI)
Der "X"-Schliessen-Button existierte bisher nur lokal in `SettingsUI.cpp`.
Mit `InfoUI` als zweitem Verbraucher nach `UiHelpers.h/.cpp` verschoben -
gleiches Muster wie zuvor bei `hitRect`/`waitForTapEvent` (dort ebenfalls
erst bei einem zweiten Verbraucher ausgelagert, nicht vorzeitig).

### Info-Dialog nutzt das bereits bestehende `deviceName()`, keine neue Einstellung
Die urspruengliche Frage war, ob fuer den Info-Dialog ein neuer
"Systemname" fuer das Display selbst noetig ist. `SettingsManager` hatte
bereits ein `deviceName()`-Feld (Default "Sensormeter Display", ueber das
Webinterface als "Systemname" editierbar, bisher nur fuer Seitentitel/
Ueberschrift der Weboberflaeche genutzt) - dieses wird jetzt zusaetzlich im
Info-Dialog angezeigt, keine neue Einstellung noetig.

### Static-IP: eigenes Formular mit sofortigem Neustart
Netzwerkeinstellungen (`WiFi.config()`) wirken erst bei der naechsten
Verbindung, nicht auf eine bereits bestehende Session - das Speichern der
Static-IP-Einstellungen im Webinterface loest daher bewusst einen
sofortigen `ESP.restart()` aus (wie beim OTA-Update), statt den Nutzer im
Unklaren zu lassen, warum die Aenderung "nicht wirkt". Gespeichert im
selben NVS-Namespace wie die WLAN-Zugangsdaten (`WlanManager`, nicht
`SettingsManager`), da es sich um Verbindungskonfiguration handelt, nicht
um eine App-Einstellung.

## RGB-Status-LED leuchtete dauerhaft trotz "aus"

### Bug gefunden und behoben: LED-Polaritaet umgekehrt (gemeinsame Anode statt Kathode)
Nutzer meldete, die RGB-Status-LED leuchte durchgehend, obwohl kein
Ping-Alarm aktiv war. `LedManager` ging bisher von gemeinsamer Kathode aus
(HIGH=an, siehe fruehere, im Code selbst als unverifiziert markierte
Annahme aus P7/P8). Tatsaechlich gemeinsame Anode (LOW=an) - im
vermeintlichen "aus"-Zustand (alle drei Pins LOW) waren dadurch alle drei
Farbkanaele gleichzeitig an. Behoben durch Invertieren der Pegel in
`LedManager::setColor()`; am Geraet verifiziert (LED jetzt tatsaechlich aus
ohne aktiven Alarm).

## Farbtest (Rot/Gelb/Blau/Gruen/Weiss): echte Ursache gefunden, RGB/BGR-Diagnose war eine Sackgasse

### Korrektur zum Abschnitt "TFT_RGB_ORDER falsch konfiguriert" weiter unten
Die dortige Diagnose und der "Fix" (`TFT_RGB_ORDER=TFT_RGB`) waren **falsch**.
Systematischer Test mit vollflaechigem Hintergrund in Rot/Gelb/Gruen/Blau/
Weiss deckte auf:

1. `TFT_RGB_ORDER=TFT_RGB`/`TFT_BGR` als Build-Flag ist bei
   `USER_SETUP_LOADED=1` wirkungslos: `User_Setup_Select.h` definiert die
   Token `TFT_RGB`/`TFT_BGR` (1/0) nur, wenn `USER_SETUP_LOADED` NICHT
   gesetzt ist - bei uns aber schon. Der Praeprozessor wertet
   `TFT_RGB_ORDER == 1` dadurch immer als `false` (undefiniertes Token = 0),
   **unabhaengig davon, ob dort `TFT_RGB` oder `TFT_BGR` steht** - verifiziert
   per `gcc -E`. Der fruehere "Fix" auf `TFT_RGB` hat also nie etwas
   veraendert, die MADCTL-Farbreihenfolge war die ganze Zeit BGR.
2. Nachdem das behoben wurde (numerischer Wert `TFT_RGB_ORDER=1` statt
   Token), zeigte sich: Rot->Gelb, Gelb->Rot, Gruen->Rosa, Cyan->Blau,
   Magenta->Gruen, Blau->Blau (korrekt) - UND **Weiss->Schwarz**. Ein
   Rechenmodell aus den Messpunkten (`OutR = NICHT B`, `OutB = NICHT R`,
   `OutG = R UND NICHT G`) erklaerte alle sechs Buntfarb-Messungen exakt,
   sagte aber auch voraus, dass **kein** Eingabewert reines Weiss erzeugen
   kann - bestaetigt durch den Test. Reines Zuruecksetzen auf
   `TFT_RGB_ORDER=0` (BGR, numerisch diesmal korrekt wirksam) behob das
   Weiss-Problem NICHT (weiterhin Schwarz) - die MADCTL-Reihenfolge war also
   nie die eigentliche Ursache, nur ein Nebenschauplatz.

### Bug gefunden und behoben: TFT_eSPI schaltet INVON standardmaessig ein
Tatsaechliche Ursache: `TFT_Drivers/ST7789_Init.h` sendet im Standard-
Initialisierungsablauf unbedingt den Befehl `ST7789_INVON` (Farbinvertierung
an). Dieses konkrete Panel braucht das nicht/nicht so - mit `INVON` aktiv
wurde Weiss zu Schwarz und alle Buntfarben verfaelscht dargestellt. Behoben
mit einem expliziten `tft.invertDisplay(false)` direkt nach `tft.init()` in
`DisplayManager::begin()`. Nach dem Fix (kombiniert mit `TFT_RGB_ORDER=0`,
siehe oben) wurden Weiss, Schwarz, Rot, Gruen und Blau alle korrekt am
echten Geraet verifiziert - keine Farbsubstitutionen mehr noetig,
`TFT_RED`/`TFT_GREEN`/etc. koennen unveraendert verwendet werden.

**Fuer spaetere Fehlersuche:** Dieser Bug erklaert rueckwirkend vermutlich
auch den weiter unten dokumentierten "TFT_LIGHTGREY praktisch unsichtbar"-
Befund - mit aktivem `INVON` waere der (vermeintlich weisse) Hintergrund in
Wirklichkeit schwarz gewesen, und invertiertes Hellgrau (~definitionsgemaess
dunkel) haette auf einem tatsaechlich schwarzen Hintergrund ebenfalls kaum
Kontrast gehabt - ein in sich konsistentes Bild, nur mit vertauschter
Ausgangsannahme (es war nie "Hellgrau auf Weiss", sondern vermutlich
"dunkles Grau auf Schwarz").

### Statusleisten-Symbolfarbe: zurueck zu Hellgrau moeglich, dann auf Dunkelgrau angepasst
Mit behobener Invertierung wurde `TFT_LIGHTGREY` (lastenheft-konform) erneut
getestet und war diesmal tatsaechlich sichtbar (nicht mehr mit dem
Hintergrund verschmolzen) - bestaetigt, dass der fruehere "Schwarz"-Kompromiss
durch den INVON-Bug verursacht war, nicht durch eine generelle
Panel-Schwaeche. Nutzer fand den Kontrast trotzdem noch zu schwach; auf
`TFT_DARKGREY` (128,128,128) gewechselt - naeher am Lastenheft-Wortlaut
"hellgrau" als das vorherige Schwarz, aber mit ausreichend Kontrast auf
Weiss.

### Zahnrad-Symbol: erst Schraubenschluessel probiert, dann verworfen
Auf Nutzerwunsch testweise durch ein stilisiertes Schraubenschluessel-Symbol
ersetzt (diagonaler Schaft mit zwei unterschiedlich grossen Ringkoepfen,
angelehnt an eine vom Nutzer bereitgestellte Referenzgrafik). Bei der
kleinen Symbolgroesse (~22px) wirkte das Ergebnis nicht ueberzeugend genug
und wurde auf Nutzerwunsch wieder verworfen. Stattdessen ein kraeftigeres
Zahnrad: gefuellter Ring (vorher duenne Kreislinien) mit ausgeschnittener
Mitte (`fillCircle` in `bgColor` ueber dem massiven Ring) und Zaehnen als
gefuellte Dreiecke statt duenner Linien - Ring-Innenradius `r-5` (nach
Nutzer-Iteration ueber `r-4`, `r-7`) fuer eine "fleischigere" Optik.

## Bildschirm "zitterte": bedingungslose Full-Redraws statt Redraw-nur-bei-Aenderung

### Bug gefunden und behoben: sichtbares Flackern durch unnoetige Redraws
Nach dem Statusleisten-Sichtbarkeits-Fix meldete der Nutzer sichtbares
Zittern/Flackern des Displays im Normalbetrieb. Ursache: mehrere
Zeichenfunktionen fuehrten bei jedem Aufruf bedingungslos einen vollen
`fillRect` + Neuzeichnen aus, auch wenn sich der angezeigte Wert seit dem
letzten Aufruf gar nicht geaendert hatte - der fillRect-dann-Text-Ablauf
brauchte messbar lange genug (SPI-Uebertragung), um als kurzes Aufblitzen
sichtbar zu sein:

- `StatusBar::draw()` wurde alle 300ms aufgerufen (noetig fuers WLAN-Blink-
  Symbol, siehe aeltere Notiz oben in dieser Datei), zeichnete aber bei
  jedem Aufruf blind neu, auch wenn WLAN-Balken/Temperatur/Uhrzeit exakt
  gleich blieben.
- Der Hauptloop erzwang zusaetzlich alle 5s (`kPeriodicRedrawIntervalMs`)
  einen Redraw des Inhaltsbereichs unabhaengig von der aktiven Datenquelle -
  noetig eigentlich nur fuer die Uhrzeit-Ansicht (deren Anzeige sich ohne
  neue Messung rein zeitgesteuert aendert), traf aber auch Graph/Ping-
  Ansichten, die bereits ueber tatsaechliche Poll-Ereignisse redraw-getriggert
  werden.
- `PingView::drawAverage()`/`drawTargetList()` (Ping-Poll alle 2s) und
  `GraphManager::drawFullScreen()` (DHT11-Poll alle 5s) zeichneten bei jedem
  Poll neu, auch wenn der gerundete Anzeigewert unveraendert war - bei Ping
  alle 2s am staerksten wahrnehmbar.

Behoben nach demselben Muster in allen vier Stellen: letzten gezeichneten
Zustand zwischenspeichern (gerundete Werte bzw. eine zusammengesetzte
Signatur bei der Ping-Zielliste) und den eigentlichen Redraw nur ausfuehren,
wenn sich etwas tatsaechlich geaendert hat. Fuer `StatusBar` bleibt die
WLAN-Blink-Animation (`bars < 0`) als Ausnahme bestehen, die weiterhin bei
jedem 300ms-Tick neu zeichnet. `periodicDue` im Hauptloop ist jetzt auf
`activeSource == DataSource::Uhrzeit` beschraenkt.

**Noch nicht angefasst:** `SensormeterView::draw()` hat denselben
bedingungslosen Redraw-bei-jedem-Poll-Aufbau, aber bei einem 30s-Poll-
Intervall (`SensormeterClient::kPollIntervalMs`) faellt das kaum auf -
bewusst nicht ungetestet geaendert, da aktuell kein Sensormeter-Geraet zum
Verifizieren erreichbar war. Bei Gelegenheit nachziehen, falls dort
ebenfalls Flackern auffaellt.

## Erster echter Test im Hauptbetrieb (nach WLAN-Verbindung): Statusleisten unsichtbar

### Bug gefunden und behoben: TFT_RGB_ORDER falsch konfiguriert
Nach erfolgreicher WLAN-Verbindung (erster Durchlauf des Hauptloops auf
echter Hardware) war die obere UND untere Statusleiste komplett
unsichtbar/schwarz, obwohl `StatusBar::draw()` nachweislich korrekt und mit
plausiblen Werten aufgerufen wurde (per Serial-Debug verifiziert). Test mit
knallrotem statt hellgrauem Symbol zur Eingrenzung: `TFT_RED` erschien auf
dem echten Panel als **Türkis**, nicht Rot - klassischer Hinweis auf
vertauschte Farbkanal-Reihenfolge. `platformio.ini` hatte
`TFT_RGB_ORDER=TFT_BGR` gesetzt, das Panel ist aber tatsaechlich RGB-nativ.
Behoben durch Aendern auf `TFT_RGB_ORDER=TFT_RGB`.

**Wichtig:** Dieser Fehler war nicht auf die Statusleiste beschraenkt,
sondern haette jede explizite Farbe mit ungleichen R/G/B-Anteilen falsch
dargestellt - u. a. den roten Alarm-Hintergrund bei Ping-Ausfall
(lastenheft.txt Abschnitt 9) sowie die Graph-Farben (Temperatur rot,
Luftfeuchte blau). Reine Grautoene (R≈G≈B) sind von diesem Fehler nicht
sichtbar betroffen, weshalb er beim allgemeinen Boot-Bildschirm-Test zuvor
nicht auffiel.

### Symbolfarbe von hellgrau auf schwarz geaendert (Abweichung vom Lastenheft)
`lastenheft.txt` Abschnitt 4 verlangt "hellgrau" fuer die Statusleisten-
Symbole. Auch nach dem RGB/BGR-Fix blieb die Original-`TFT_LIGHTGREY`-Farbe
auf dem echten 2,8"-ST7789P3-Panel praktisch nicht von Weiss unterscheidbar
(Hardware-Befund, vermutlich begrenzte Graustufen-Differenzierung dieses
guenstigen Panels). Auf schwarz umgestellt fuer tatsaechliche Lesbarkeit -
bewusste Abweichung vom woertlichen Lastenheft-Wortlaut zugunsten der
dahinterliegenden Absicht (eine erkennbare Statusleiste). Interessanter
Nebenbefund: `TFT_BLACK` erscheint auf diesem Panel selbst als **weiss**
(vermutlich Interaktion mit der Panel-eigenen Inversionseinstellung) - das
Ergebnis ist trotzdem gut lesbar (weisse Symbole, siehe Test durch Nutzer),
daher nicht weiter verfolgt. Graph-Linienfarben (rot/blau) und der rote
Alarm-Hintergrund sind nach dem RGB/BGR-Fix noch nicht am echten Geraet
verifiziert (noch nicht genug DHT11-Messwerte fuer den Graph vorhanden) -
bei Gelegenheit nachpruefen.

## Erster echter Touch-Test: X/Y-Kanal vertauscht

### Bug gefunden und behoben: Touch-Controller liefert X/Y vertauscht
Beim ersten Test der neuen 4-Punkt-Kalibrierung auf echter Hardware kollabierten
`xMin`/`xMax` bzw. `yMin`/`yMax` immer wieder auf nahezu denselben Wert
(wenige Rohwert-Einheiten Unterschied), unabhaengig von Tippgenauigkeit,
Zielgroesse oder Haltedauer der Beruehrung - drei nacheinander ausprobierte
Erklaerungen (unzureichend eingeschwungener Rohwert, zu kleines/schwer
treffbares Kalibrierziel, zu kurzer Tipp) brachten keine Besserung.

Per-Sample-Debug-Log (`docs`/Kommentare in `TouchManager.cpp` zwischenzeitlich
entfernt, siehe Git-Historie) zeigte den eigentlichen Grund: Kommando 0xD0
(`kCmdReadX`) liefert auf dieser Verkabelung tatsaechlich die **vertikale**
Position, Kommando 0x90 (`kCmdReadY`) die **horizontale** - die beiden Kanaele
sind gegenueber der XPT2046-Standardannahme vertauscht. Die 4-Punkt-Mittelung
mittelte dadurch z. B. "oben-links + unten-links" (fuer `xMin` gedacht), was
auf dieser Verkabelung in Wirklichkeit eine hohe und eine niedrige
*vertikale* Ablesung sind - kollabiert folgerichtig auf einen bedeutungslosen
Mittelwert, komplett unabhaengig von der echten X-Position.

Behoben durch Vertauschen der Kanalzuweisung in `TouchManager::readRaw()`
(`rawX = readChannel(kCmdReadY)`, `rawY = readChannel(kCmdReadX)`) statt die
Kommandokonstanten selbst umzubenennen - die Namen `kCmdReadX`/`kCmdReadY`
bleiben an den XPT2046-Standardnamen orientiert, nicht an dieser
konkreten Verkabelung. Nach dem Fix stimmen alle vier Kalibrierecken und
freie Tipps praezise mit der angetippten Bildschirmposition ueberein
(auf echter Hardware verifiziert).

Die waehrend der Fehlersuche ergaenzte Setzzeit+Mittelung in `waitForTap()`
sowie das groessere Fadenkreuz-Kalibrierziel (statt 4px-Punkt) waren nicht
die Ursache, bleiben aber als sinnvolle Robustheit gegen einzelne Ausreisser
bzw. schlecht treffbare Ziele bestehen.

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

## Versionierung

Anders als die beiden Schwesterprojekte hatte dieses Repo bislang **gar
keine** Firmware-Versionskonstante - keine `config.h`/`config.h.example`,
kein `DEVICE_FIRMWARE_VERSION`, keine Anzeige der Version an irgendeiner
Stelle (weder Touch-UI noch Webserver), obwohl die Weboberfläche bereits
einen Link auf "Releases auf GitHub" zeigt.

**Nachgezogen, analog zu Sensormeter und Sensormeter WLAN:**
- `firmware/include/config.h.example` (+lokale `config.h`, per
  `.gitignore` bereits vorbereitet) neu angelegt mit
  `DEVICE_FIRMWARE_VERSION "0.9.0-rc1"` (Beta) - gleicher Stand wie die
  beiden anderen Projekte
- **Touch-UI**: Version nur im Geräte-Info-Dialog (`InfoUI.cpp`, hinter dem
  i-Symbol) als vierte Zeile ergänzt - bewusst nicht auf anderen Screens,
  da dort kein Platz/Bedarf für eine reine Diagnose-Info besteht
  (Nutzerentscheidung)
- **Webserver**: im Bereich „Firmware-Update" als „Aktuell installiert:
  …" ergänzt (`WebServerManager.cpp`)

Versionsnummer zusätzlich in README, One-Pager und Admin-Guide vermerkt,
wie bei den beiden anderen Projekten. Git-Tags/GitHub-Releases mit
`.bin`-Artefakt sind - wie dort auch - noch nicht Teil dieser Änderung.
