@echo off
title SolarController - Giao Tiep ESP32 va Thiet Bi Dien
chcp 65001 > nul
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1

cd /d "%~dp0"
mode con: cols=100 lines=36 >nul 2>&1
color 0B

:: 1. Uu tien su dung Python Portable da dong goi san trong Software\Python\runtime
set "PORTABLE_PY=%~dp0Software\Python\runtime\python.exe"
set "VENV_PY=%~dp0Software\Python\.venv\Scripts\python.exe"
set "APP_PYTHON="

if exist "%PORTABLE_PY%" (
    set "APP_PYTHON=%PORTABLE_PY%"
) else if exist "%VENV_PY%" (
    set "APP_PYTHON=%VENV_PY%"
) else (
    :: Tim Python tren he thong
    python --version >nul 2>&1
    if not errorlevel 1 (
        set "APP_PYTHON=python"
    ) else (
        py --version >nul 2>&1
        if not errorlevel 1 (
            set "APP_PYTHON=py"
        )
    )
)

if "%APP_PYTHON%"=="" (
    color 0C
    echo ========================================================================
    echo  [LOI] Khong tim thay Python tren he thong!
    echo  Vui long cai dat Python 3.8+ tu https://www.python.org/
    echo  Luu y: Nho tich chon "Add Python to PATH" khi cai dat.
    echo ========================================================================
    pause
    exit /b 1
)

:: Kiem tra va cai dat thu vien neu dung Python ngoai
"%APP_PYTHON%" -c "import serial, requests, rich" >nul 2>&1
if errorlevel 1 (
    echo [THONG BAO] Dang cai dat cac thu vien con thieu: pyserial, requests, rich...
    if exist "%~dp0Software\Python\requirements.txt" (
        "%APP_PYTHON%" -m pip install -q -r "%~dp0Software\Python\requirements.txt"
    ) else if exist "%~dp0Software\requirements.txt" (
        "%APP_PYTHON%" -m pip install -q -r "%~dp0Software\requirements.txt"
    ) else if exist "%~dp0Code Base\python\requirements.txt" (
        "%APP_PYTHON%" -m pip install -q -r "%~dp0Code Base\python\requirements.txt"
    )
)

:: 2. Tim file main.py cua ung dung
set "MAIN_PY="
if exist "%~dp0Software\Python\main.py" (
    set "MAIN_PY=%~dp0Software\Python\main.py"
) else if exist "%~dp0Software\main.py" (
    set "MAIN_PY=%~dp0Software\main.py"
) else if exist "%~dp0Code Base\python\main.py" (
    set "MAIN_PY=%~dp0Code Base\python\main.py"
)

if "%MAIN_PY%"=="" (
    color 0C
    echo ========================================================================
    echo  [LOI] Khong tim thay file main.py trong Software/Python hoac Code Base/python!
    echo ========================================================================
    pause
    exit /b 1
)

:MENU
cls
color 0B
echo ========================================================================================
echo          HE THONG GIAO TIEP ESP32 ^& GIAM SAT PIN MAT TROI (SOLAR CONTROLLER)
echo                                  Version v1.1.0
echo ========================================================================================
echo.
echo    [1] Hien thi trang thai tong the (Realtime CLI Monitor)
echo        - 4 Module LDR: Thong so quang tro, do sang, do lech Trai - Phai
echo        - Dien ap tieu thu: Do dien ap pin / he thong (Vbat, Voltage)
echo        - Luong pin con lai: Phan tram dung luong (SOC), thanh pin truc quan
echo        - Huong tam pin: Goc xoay Servo (0 - 180 do), huong Dong/Tay, che do Auto/Manual
echo.
echo    [2] Cai dat WiFi cho ESP32 (Setting WiFi)
echo        - Cau hinh ten mang (SSID) ^& mat khau WiFi moi cho ESP32
echo        - Tu dong gui qua Serial/Cloud, luu Flash NVS, tu phuc hoi neu loi
echo.
echo    [3] Thoat chuong trinh (Exit)
echo.
echo ========================================================================================
echo.

set "choice="
set /p choice="Nhap lua chon cua ban [1, 2, 3]: "

if "%choice%"=="1" goto OPTION_MONITOR
if "%choice%"=="2" goto OPTION_WIFI
if "%choice%"=="3" goto OPTION_EXIT
if /i "%choice%"=="q" goto OPTION_EXIT

echo.
echo [!] Lua chon khong hop le, vui long chi nhap 1, 2 hoac 3.
ping 127.0.0.1 -n 2 > nul
goto MENU

:OPTION_MONITOR
cls
"%APP_PYTHON%" "%MAIN_PY%" --action monitor
if errorlevel 1 (
    echo.
    echo [THONG BAO] Tien trinh da dung hoac co loi. Nhan phim bat ky de quay lai menu...
    pause > nul
)
goto MENU

:OPTION_WIFI
cls
"%APP_PYTHON%" "%MAIN_PY%" --action wifi
if errorlevel 1 (
    echo.
    echo [THONG BAO] Tien trinh da dung hoac co loi. Nhan phim bat ky de quay lai menu...
    pause > nul
)
goto MENU

:OPTION_EXIT
cls
echo.
echo ========================================================================================
echo   Cam on ban da su dung phan mem SolarController! Dang thoat...
echo ========================================================================================
ping 127.0.0.1 -n 2 > nul
exit /b 0
