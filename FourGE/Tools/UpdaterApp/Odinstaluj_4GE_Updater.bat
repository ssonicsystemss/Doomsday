@echo off
rem ==== 4GE UPDATER - usuniecie (sam plugin 4GE zostaje) ====
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
schtasks /Delete /F /TN "4GE Updater" >nul 2>&1
taskkill /F /IM "4GE Updater.exe" >nul 2>&1
powershell -NoProfile -Command "Remove-Item ([Environment]::GetFolderPath('Desktop')+'\4GE Updater.lnk'),([Environment]::GetFolderPath('Desktop')+'\Sprawdz aktualizacje 4GE.lnk') -ErrorAction SilentlyContinue"
rmdir /S /Q "%LOCALAPPDATA%\4GE_Updater" 2>nul
echo 4GE Updater usuniety (plugin 4GE zostal na miejscu).
pause
