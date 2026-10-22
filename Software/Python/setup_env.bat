@echo off
title SolarController - Khoi Tao Moi Truong Python
chcp 65001 > nul
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1
cd /d "%~dp0"

echo ========================================================================
echo   Dang kiem tra moi truong Python cho SolarController...
echo ========================================================================

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [LOI] Khong tim thay Python tren he thong!
    echo Vui long cai dat Python 3.8+ tu https://www.python.org/downloads/
    echo va nho tich vao o "Add Python to PATH" khi cai dat.
    pause
    exit /b 1
)

if not exist ".venv\Scripts\python.exe" (
    echo Dang tao moi truong ao (.venv)...
    python -m venv .venv
    if errorlevel 1 (
        echo [CANH BAO] Khong the tao virtual environment, se su dung Python toan cuc.
        python -m pip install -r requirements.txt
        exit /b 0
    )
)

echo Dang cai dat/cap nhat thu vien can thiet: pyserial, requests, rich...
if exist ".venv\Scripts\python.exe" (
    .venv\Scripts\python.exe -m pip install -q -r requirements.txt
)
if errorlevel 1 (
    echo [CANH BAO] Co loi khi cai dat thu vien vao venv, thu dung pip toan cuc...
    python -m pip install -r requirements.txt
)

echo [OK] Moi truong Python da san sang!
