@echo off
rem ==== 4GE UPDATER - instalacja (dwuklik, raz) ====
rem Prosi raz o prawa administratora. Kopiuje program do %LOCALAPPDATA%\4GE_Updater, ustawia sprawdzanie co 10 minut (Harmonogram zadan, bez okienek),
rem dodaje na pulpit ikone "Sprawdz aktualizacje 4GE" i od razu sprawdza.
setlocal
rem prawa administratora RAZ (zadanie w tle podmienia potem 4GE w Program Files bez pytania)
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
set "D=%LOCALAPPDATA%\4GE_Updater"
if not exist "%D%" mkdir "%D%"

if exist "%~dp04GE_Updater.ps1" (
    copy /Y "%~dp04GE_Updater.ps1" "%D%\4GE_Updater.ps1" >nul
) else (
    echo Pobieram program z GitHuba...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing 'https://raw.githubusercontent.com/ssonicsystemss/Doomsday/main/FourGE/Tools/Updater/4GE_Updater.ps1' -OutFile '%D%\4GE_Updater.ps1'"
)
if not exist "%D%\4GE_Updater.ps1" (
    echo Nie udalo sie pobrac programu. Sprawdz internet i sprobuj jeszcze raz.
    pause
    exit /b 1
)
powershell -NoProfile -Command "Unblock-File -Path '%D%\4GE_Updater.ps1'" >nul 2>&1

rem cichy start (bez migajacego okna PowerShella)
> "%D%\run_hidden.vbs" echo Set sh = CreateObject("WScript.Shell")
>> "%D%\run_hidden.vbs" echo args = ""
>> "%D%\run_hidden.vbs" echo If WScript.Arguments.Count ^> 0 Then args = " " ^& WScript.Arguments(0)
>> "%D%\run_hidden.vbs" echo sh.Run "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ ^& sh.ExpandEnvironmentStrings("%%LOCALAPPDATA%%") ^& "\4GE_Updater\4GE_Updater.ps1""" ^& args, 0, False

schtasks /Create /F /RL HIGHEST /TN "4GE Updater" /SC MINUTE /MO 10 /TR "wscript.exe \"%D%\run_hidden.vbs\" -Quiet" >nul
if errorlevel 1 (
    echo Nie udalo sie ustawic sprawdzania co 10 minut ^(Harmonogram zadan^).
) else (
    echo Sprawdzanie co 10 minut: ustawione.
)

powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Desktop')+'\Sprawdz aktualizacje 4GE.lnk'); $s.TargetPath='wscript.exe'; $s.Arguments='\"%D%\run_hidden.vbs\"'; $s.IconLocation='%SystemRoot%\System32\shell32.dll,238'; $s.Save()"
echo Ikona na pulpicie: "Sprawdz aktualizacje 4GE".
echo.
echo Sprawdzam teraz (i od razu podmieniam, jesli jest nowszy 4GE)...
schtasks /Run /TN "4GE Updater" >nul
echo Gotowe. To okno mozesz zamknac.
pause
