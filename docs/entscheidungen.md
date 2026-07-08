# Entscheidungsprotokoll — Sensormeter Display

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
