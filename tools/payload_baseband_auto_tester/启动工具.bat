@echo off
setlocal
cd /d "%~dp0"
if not exist "bridge\jtool_spi_bridge.exe" powershell -ExecutionPolicy Bypass -File ".\build-bridge.ps1"
python -c "import serial, paho.mqtt.client, openpyxl" >nul 2>nul
if errorlevel 1 python -m pip install -r requirements.txt
python app.py %*
set "APP_RC=%ERRORLEVEL%"
if not "%APP_RC%"=="0" if "%~1"=="" pause
exit /b %APP_RC%
