@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

if not exist ".venv\Scripts\python.exe" (
    echo [1/2] Dang tao moi truong Python (.venv)...
    python -m venv .venv
    if errorlevel 1 goto :err
    ".venv\Scripts\python.exe" -m pip install --upgrade pip >nul 2>&1
    echo [2/2] Dang cai thu vien (pyserial, requests, rich, colorama)...
    ".venv\Scripts\python.exe" -m pip install -r requirements.txt
    if errorlevel 1 goto :err
)

".venv\Scripts\python.exe" main.py %*
goto :eof

:err
echo.
echo [LOI] Khong the thiet lap moi truong Python. Kiem tra Python 3 da cai va co trong PATH.
pause
exit /b 1
