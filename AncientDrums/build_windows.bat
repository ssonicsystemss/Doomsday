@echo off
REM ==========================================================
REM  ANCIENT DRUMS - build VST3 for Windows
REM  Wymaga: Visual Studio 2022 (lub Build Tools) z "Desktop development with C++"
REM          + CMake (jest w VS) + Git
REM ==========================================================
cd /d "%~dp0"

cmake -B build -A x64
if errorlevel 1 goto fail

cmake --build build --config Release --target AncientDrums_VST3 --parallel
if errorlevel 1 goto fail

echo.
echo Gotowe! Plugin:
echo   %~dp0build\AncientDrums_artefacts\Release\VST3\Ancient Drums.vst3
echo.
echo Skopiuj caly folder "Ancient Drums.vst3" do:
echo   C:\Program Files\Common Files\VST3\
echo.
pause
exit /b 0

:fail
echo.
echo BLAD budowania - sprawdz czy masz Visual Studio 2022 z C++ i Git.
pause
exit /b 1
