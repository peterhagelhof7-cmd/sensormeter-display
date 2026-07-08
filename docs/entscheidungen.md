# Entscheidungsprotokoll — Sensormeter Display

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
