@echo off
echo ========================================
echo ESP32-P4 FLASH AND TEST
echo ========================================
echo.

cd /d %~dp0

echo Activating ESP-IDF 6.0...
call C:\esp\v6.0.2\esp-idf\export.bat

echo.
echo ========================================
echo FLASHING FIRMWARE TO COM4
echo ========================================
idf.py -p COM4 flash

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo FLASH FAILED!
    echo ========================================
    pause
    exit /b 1
)

echo.
echo ========================================
echo FLASH SUCCESSFUL!
echo ========================================
echo.
echo Starting monitor (Press Ctrl+] to exit)...
echo.
timeout /t 3

idf.py -p COM4 monitor
