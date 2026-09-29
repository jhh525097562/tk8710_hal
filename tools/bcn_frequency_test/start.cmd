@echo off
cd /d "%~dp0\..\.."
python tools\bcn_frequency_test\run.py --password %*
pause
