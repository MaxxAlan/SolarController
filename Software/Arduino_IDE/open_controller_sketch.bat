@echo off
title SolarController - Mo Firmware trong Arduino IDE
chcp 65001 > nul
cd /d "%~dp0"

set "SKETCH_PATH=%~dp0..\..\Code Base\arduino-controller\controller\controller.ino"

echo ========================================================================
echo   Dang mo file firmware ESP32: controller.ino
echo   Duong dan: %SKETCH_PATH%
echo ========================================================================

if exist "D:\software\ArduinoIDE\Arduino IDE\Arduino IDE.exe" (
    start "" "D:\software\ArduinoIDE\Arduino IDE\Arduino IDE.exe" "%SKETCH_PATH%"
    exit /b 0
)

if exist "%LOCALAPPDATA%\Programs\Arduino IDE\Arduino IDE.exe" (
    start "" "%LOCALAPPDATA%\Programs\Arduino IDE\Arduino IDE.exe" "%SKETCH_PATH%"
    exit /b 0
)

if exist "%ProgramFiles%\Arduino IDE\Arduino IDE.exe" (
    start "" "%ProgramFiles%\Arduino IDE\Arduino IDE.exe" "%SKETCH_PATH%"
    exit /b 0
)

if exist "%ProgramFiles(x86)%\Arduino\arduino.exe" (
    start "" "%ProgramFiles(x86)%\Arduino\arduino.exe" "%SKETCH_PATH%"
    exit /b 0
)

:: Neu khong tim thay duong dan mac dinh, goi Windows mo file .ino
start "" "%SKETCH_PATH%"
echo [OK] Da gui lenh mo file firmware.
