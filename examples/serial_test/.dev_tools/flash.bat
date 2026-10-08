@echo off
REM ESP32-P4 Test Firmware Flash Script
REM Project: esp-idf-espure Scientific Verification
REM Device: ESP32-P4 Rev 1.3 on COM4

echo ============================================================
echo ESP32-P4 Flash to COM4
echo ============================================================

REM Set ESP-IDF environment
echo Setting up ESP-IDF 6.0.2 environment...
call C:\esp\v6.0.2\esp-idf\export.bat
if errorlevel 1 (
    echo ERROR: Failed to setup ESP-IDF environment
    exit /b 1
)

REM Check if build exists
if not exist build (
    echo ERROR: Build directory not found. Run build.bat first
    exit /b 1
)

REM Flash to ESP32-P4
echo.
echo Flashing to ESP32-P4 on COM4...
idf.py -p COM4 flash

if errorlevel 1 (
    echo.
    echo ============================================================
    echo FLASH FAILED
    echo ============================================================
    exit /b 1
) else (
    echo.
    echo ============================================================
    echo FLASH SUCCESSFUL
    echo Use monitor_to_log.bat to capture output
    echo ============================================================
)
