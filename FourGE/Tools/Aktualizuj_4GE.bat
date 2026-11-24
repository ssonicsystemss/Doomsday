@echo off
rem ==== 4GE - pobiera najnowszy testowy build z GitHuba (Releases "4ge-latest") i instaluje go do VST3 ====
rem Dwuklik. Prosi o uprawnienia administratora (folder C:\Program Files\Common Files\VST3).

net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

tasklist /FI "IMAGENAME eq FL64.exe" | find /I "FL64.exe" >nul
if not errorlevel 1 (
    echo.
    echo  FL Studio jest otwarte - zamknij FL i uruchom ten plik jeszcze raz.
    echo  ^(otwarty FL trzyma stary plik 4GE.vst3 i nie da go podmienic^)
    echo.
    pause
    exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$tmp = Join-Path $env:TEMP '4GE_update';" ^
  "if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force };" ^
  "New-Item -ItemType Directory $tmp | Out-Null;" ^
  "$zip = Join-Path $tmp '4GE.zip';" ^
  "Write-Host 'Pobieram najnowszy 4GE...';" ^
  "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;" ^
  "Invoke-WebRequest -UseBasicParsing 'https://github.com/ssonicsystemss/Doomsday/releases/download/4ge-latest/4GE-VST3-Windows.zip' -OutFile $zip;" ^
  "Expand-Archive $zip -DestinationPath $tmp -Force;" ^
  "$src = Get-ChildItem $tmp -Directory -Filter '4GE.vst3' -Recurse | Select-Object -First 1;" ^
  "if (-not $src) { throw 'w paczce nie ma 4GE.vst3' };" ^
  "$dst = 'C:\Program Files\Common Files\VST3\4GE.vst3';" ^
  "if (Test-Path $dst) { Remove-Item $dst -Recurse -Force };" ^
  "Copy-Item $src.FullName $dst -Recurse;" ^
  "try { $r = Invoke-RestMethod 'https://api.github.com/repos/ssonicsystemss/Doomsday/releases/tags/4ge-latest'; Write-Host ''; Write-Host $r.body } catch {};" ^
  "Write-Host ''; Write-Host 'GOTOWE - 4GE zainstalowany. Mozesz otworzyc FL.'"
if errorlevel 1 echo. & echo  Cos poszlo nie tak - wyslij Claude'owi tekst z tego okna.
echo.
pause
