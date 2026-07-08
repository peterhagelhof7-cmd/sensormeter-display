# scripts/

## flash-sensormeter-display.ps1 / flash-sensormeter-display.cmd

Richtet auf einem beliebigen Windows-PC alles ein, was für einen
Flash-Vorgang nötig ist, und flasht anschließend:

1. Python installieren (falls nicht vorhanden, über winget)
2. Git installieren (falls nicht vorhanden, über winget)
3. PlatformIO installieren (falls nicht vorhanden, über pip)
4. Repo klonen (falls noch nicht vorhanden) bzw. aktualisieren
   (`git pull`, nur wenn keine lokalen Änderungen vorliegen)
5. Firmware bauen (`pio run`)
6. Firmware flashen (`pio run --target upload`)

Anders als beim Sensormeter-Projekt gibt es keine `config.h` anzulegen —
alle Einstellungen (WLAN, Betriebsmodus, Sensormeter-Ziel, Ping-Ziele)
werden am Gerät selbst per Touch eingerichtet und in NVS gespeichert.

**Voraussetzung:** Board per USB-C oder USB-Micro angeschlossen (siehe
`CBAA0055-008_DE.pdf`), ggf. CH340-Treiber installiert.

### Nutzung

Nur diese eine Datei `flash-sensormeter-display.ps1` (oder zusätzlich die
`.cmd` zum Doppelklicken) auf den Ziel-PC kopieren und ausführen – das Repo
muss dafür noch **nicht** vorher geklont sein, das Skript erledigt das
selbst.

**Per Doppelklick:** `flash-sensormeter-display.cmd` – öffnet ein
Konsolenfenster, das nach Abschluss offen bleibt (zum Lesen von
Meldungen/Fehlern).

**Per PowerShell:**

```powershell
.\flash-sensormeter-display.ps1                      # Standardablauf
.\flash-sensormeter-display.ps1 -Port COM5           # fester COM-Port
.\flash-sensormeter-display.ps1 -SkipUpload          # nur bauen, nicht flashen
.\flash-sensormeter-display.ps1 -RepoPath C:\Projekte\sensormeter-display
```

Falls PowerShell die Ausführung wegen der Execution Policy verweigert:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash-sensormeter-display.ps1
```
