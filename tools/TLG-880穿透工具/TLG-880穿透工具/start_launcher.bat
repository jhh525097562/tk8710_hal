@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "LAUNCHER=%SCRIPT_DIR%RK3506TunnelLauncher.ps1"

if not exist "%LAUNCHER%" (
  echo missing launcher: %LAUNCHER%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%LAUNCHER%"
