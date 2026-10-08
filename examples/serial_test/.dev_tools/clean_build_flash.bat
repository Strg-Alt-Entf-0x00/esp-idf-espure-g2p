@echo off
echo ========================================
echo CLEAN BUILD + FLASH FOR ESP32-P4 REV 1.3
echo ========================================
echo.

cd /d %~dp0

echo Activating ESP-IDF 6.0...
call C:\esp\v6.0.2\esp-idf\export.bat

echo.
echo ========================================
echo CLEAN BUILD
echo ========================================
idf.py fullclean
idf.py build

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL - FLASHING TO COM4
echo ========================================
idf.py -p COM4 flash

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo FLASH FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! Starting monitor...
echo ========================================
timeout /t 3
idf.py -p COM4 monitor
