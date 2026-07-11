# scripts/

## flash.ps1 / flash.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend eines von vier
Sensormeter-Schwesterprojekten:

1. **Projekt wählen** – interaktiv (1-4-Menü) oder per `-Project
   sensormeter|wlan|display|poe`:
   - **Sensormeter** (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
   - **Sensormeter WLAN** (generisches ESP32-WROOM-32-DevKit, WLAN-only)
   - **Sensormeter Display** (ESP32-Touchdisplay, SNMP-Client)
   - **Sensormeter PoE** (Waveshare ESP32-S3-ETH, PoE optional, Relais)
2. Python installieren (falls nicht vorhanden, über winget)
3. Git installieren (falls nicht vorhanden, über winget)
4. PlatformIO installieren (falls nicht vorhanden, über pip)
5. Das gewählte Repo klonen (falls noch nicht vorhanden) bzw. aktualisieren
   (`git pull`, nur wenn keine lokalen Änderungen vorliegen)
6. `firmware/include/config.h` aus der Vorlage anlegen, falls das gewählte
   Projekt eine braucht und sie noch fehlt (Sensormeter Display braucht
   keine – alle Einstellungen wie WLAN, Betriebsmodus, Sensormeter-Ziel,
   Ping-Ziele werden am Gerät selbst per Touch eingerichtet und in NVS
   gespeichert)
7. Firmware bauen (`pio run`)
8. Firmware flashen (`pio run --target upload`)

Dieses Skript liegt identisch in allen vier Repos (`scripts/flash.ps1`) –
unabhängig davon, welches Projekt gerade lokal ausgecheckt ist, lässt sich
darüber jedes der vier flashen (die jeweils anderen werden bei Bedarf
automatisch in einen Unterordner neben dem Skript geklont).

**Voraussetzung für Sensormeter Display:** Board per USB-C oder USB-Micro
angeschlossen (siehe `CBAA0055-008_DE.pdf`), ggf. CH340-Treiber installiert.

### Nutzung

Nur diese eine Datei `flash.ps1` (oder zusätzlich die `.cmd` zum
Doppelklicken) auf den Ziel-PC kopieren und ausführen – das jeweilige Repo
muss dafür noch **nicht** vorher geklont sein, das Skript erledigt das
selbst.

**Per Doppelklick:** `flash.cmd` – öffnet ein Konsolenfenster, fragt nach
dem gewünschten Projekt und bleibt nach Abschluss offen (zum Lesen von
Meldungen/Fehlern).

**Per PowerShell:**

```powershell
.\flash.ps1                                  # fragt interaktiv nach dem Projekt
.\flash.ps1 -Project display                 # Projekt direkt angeben
.\flash.ps1 -Project display -Port COM5      # fester COM-Port
.\flash.ps1 -Project display -SkipUpload     # nur bauen, nicht flashen
.\flash.ps1 -Project display -RepoPath C:\Projekte\sensormeter-display
```

Falls PowerShell die Ausführung wegen der Execution Policy verweigert:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

## convert-logo.ps1

Konvertiert ein Anbieter-Logo (beliebiges Bildformat) in das fürs
Anbieter-Branding-Feature kompatible Rohformat – liegt identisch in allen
vier Repos, analog zu `flash.ps1`. Fragt zuerst (interaktiv oder per
`-Display sensormeter|wlan|poe|display|custom`), für welches Display
konvertiert werden soll, und reduziert Auflösung **und Farbtiefe**
konsequent auf das, was das jeweilige Display tatsächlich darstellen kann.
Für Sensormeter Display (TFT ST7789P3, 240×320, RGB565) bewusst als
**experimentell** markiert – dieses Projekt hat noch keine
Branding-Firmware, die das Format konsumiert; das Skript bereitet nur
schon das passende Format vor. Details siehe
[Sensormeter scripts/README.md](https://github.com/peterhagelhof7-cmd/sensormeter/blob/main/scripts/README.md)
bzw. die Kopfkommentare im Skript selbst.

```powershell
.\convert-logo.ps1 -InputPath .\firmenlogo.png -Display display
```
