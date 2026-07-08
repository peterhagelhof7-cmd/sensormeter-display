# scripts/

## flash.ps1 / flash.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend eines von drei
Sensormeter-Schwesterprojekten:

1. **Projekt wählen** – interaktiv (1/2/3-Menü) oder per `-Project
   sensormeter|wlan|display`:
   - **Sensormeter** (WT32-ETH01, Ethernet + bis zu 2 Sensoren)
   - **Sensormeter WLAN** (generisches ESP32-WROOM-32-DevKit, WLAN-only)
   - **Sensormeter Display** (ESP32-Touchdisplay, SNMP-Client)
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

Dieses Skript liegt identisch in allen drei Repos (`scripts/flash.ps1`) –
unabhängig davon, welches Projekt gerade lokal ausgecheckt ist, lässt sich
darüber jedes der drei flashen (die jeweils anderen beiden werden bei Bedarf
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
