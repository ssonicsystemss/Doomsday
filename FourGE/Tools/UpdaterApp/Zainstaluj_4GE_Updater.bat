@echo off
rem ==== 4GE UPDATER - instalacja (dwuklik, raz). Prosi raz o prawa administratora. ====
rem Program siedzi w zasobniku (ikonka 4GE przy zegarze), sam sprawdza co 10 minut i podmienia 4GE - bez wyskakujacych okienek.
setlocal
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
set "D=%LOCALAPPDATA%\4GE_Updater"
if not exist "%D%" mkdir "%D%"
if not exist "%~dp04GE Updater.exe" (
    echo Brak pliku "4GE Updater.exe" obok tego instalatora - rozpakuj caly folder z ZIP-a.
    pause
    exit /b 1
)
rem stary updater (PowerShell) i stara wersja programu - precz
schtasks /Delete /F /TN "4GE Updater" >nul 2>&1
taskkill /F /IM "4GE Updater.exe" >nul 2>&1
del /Q "%D%\4GE_Updater.ps1" "%D%\run_hidden.vbs" "%D%\new.ps1" >nul 2>&1
powershell -NoProfile -Command "Remove-Item ([Environment]::GetFolderPath('Desktop')+'\Sprawdz aktualizacje 4GE.lnk') -ErrorAction SilentlyContinue"

copy /Y "%~dp04GE Updater.exe" "%D%\4GE Updater.exe" >nul
schtasks /Create /F /RL HIGHEST /IT /RU "%USERNAME%" /TN "4GE Updater" /SC ONLOGON /TR "\"%D%\4GE Updater.exe\" --background" >nul
if errorlevel 1 (echo Nie udalo sie ustawic startu z Windows ^(Harmonogram zadan^).) else (echo Start razem z Windows: ustawiony.)
powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Desktop')+'\4GE Updater.lnk'); $s.TargetPath='%D%\4GE Updater.exe'; $s.Save()"
echo Ikona na pulpicie: "4GE Updater" (otwiera okno; program i tak dziala w tle).
schtasks /Run /TN "4GE Updater" >nul
echo.
echo Gotowe. Ikonka 4GE jest przy zegarze (moze byc schowana pod strzalka ^^).
pause
