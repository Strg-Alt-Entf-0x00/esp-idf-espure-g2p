@echo off
REM ESP32-P4 Test Firmware Build Script
REM Project: esp-idf-espure Scientific Verification
REM Device: ESP32-P4 Rev 1.3 on COM4

echo ============================================================
echo ESP32-P4 Test Firmware Build
echo ============================================================

REM Set ESP-IDF environment
echo Setting up ESP-IDF 6.0.2 environment...
call C:\esp\v6.0.2\esp-idf\export.bat
if errorlevel 1 (
    echo ERROR: Failed to setup ESP-IDF environment
    exit /b 1
)

REM Clean previous build
echo.
echo Cleaning previous build...
if exist build rmdir /s /q build

REM Build the project
echo.
echo Building project...
idf.py build

if errorlevel 1 (
    echo.
    echo ============================================================
    echo BUILD FAILED
    echo Check build\log\ for detailed error logs
    echo ============================================================
    exit /b 1
) else (
    echo.
    echo ============================================================
    echo BUILD SUCCESSFUL
    echo Ready to flash to COM4
    echo ============================================================
)
