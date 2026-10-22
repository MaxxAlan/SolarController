@echo off
title Cai Dat Thu Vien Arduino Cho SolarController
chcp 65001 > nul
cd /d "%~dp0"

echo ========================================================================
echo   DANH SÁCH THƯ VIỆN BẮT BUỘC TRONG ARDUINO IDE
echo ========================================================================
echo.
echo   1. ESP32Servo              (Bởi Kevin Harrington)
echo   2. ArduinoJson (v7.x)      (Bởi Benoit Blanchon)
echo   3. Firebase ESP Client     (Bởi Mobizt)
echo.
echo ========================================================================

set "LIB_DIR=%USERPROFILE%\Documents\Arduino\libraries"

if not exist "%LIB_DIR%" (
    mkdir "%LIB_DIR%" 2>nul
)

git --version >nul 2>&1
if %errorlevel% equ 0 (
    echo Phat hien Git! Dang tu dong cai dat thu vien vao %LIB_DIR%...
    echo.
    if not exist "%LIB_DIR%\ESP32Servo" (
        echo [1/3] Dang tai ESP32Servo...
        git clone --depth 1 https://github.com/madhephaestus/ESP32Servo.git "%LIB_DIR%\ESP32Servo" 2>nul
    ) else (
        echo [1/3] ESP32Servo da ton tai.
    )

    if not exist "%LIB_DIR%\ArduinoJson" (
        echo [2/3] Dang tai ArduinoJson...
        git clone --depth 1 https://github.com/bblanchon/ArduinoJson.git "%LIB_DIR%\ArduinoJson" 2>nul
    ) else (
        echo [2/3] ArduinoJson da ton tai.
    )

    if not exist "%LIB_DIR%\Firebase-ESP-Client" (
        echo [3/3] Dang tai Firebase-ESP-Client...
        git clone --depth 1 https://github.com/mobizt/Firebase-ESP-Client.git "%LIB_DIR%\Firebase-ESP-Client" 2>nul
    ) else (
        echo [3/3] Firebase-ESP-Client da ton tai.
    )

    echo.
    echo ========================================================================
    echo  [THANH CONG] Cac thu vien da duoc cai dat xong vao Arduino/libraries!
    echo ========================================================================
) else (
    echo May chua cai Git. Vui long mo Arduino IDE, vao Library Manager (Ctrl+Shift+I)
    echo va tim kiem cai dat 3 thu vien tren:
    echo   - ESP32Servo
    echo   - ArduinoJson
    echo   - Firebase ESP Client
)

pause
