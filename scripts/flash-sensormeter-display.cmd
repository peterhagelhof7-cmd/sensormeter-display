@echo off
rem Doppelklick-Wrapper fuer flash-sensormeter-display.ps1 - umgeht die
rem Execution-Policy-Sperre fuer unsignierte .ps1-Dateien nur fuer diesen
rem einen Aufruf.
setlocal
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%flash-sensormeter-display.ps1" %*
echo.
pause
