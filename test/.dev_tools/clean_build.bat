@echo off
REM Clean Build Script for ESP32 G2P Test
echo ========================================
echo ESP32 G2P Test - Clean Build
echo ========================================
echo.

REM Clean old build
if exist build (
    echo Cleaning old build directory...
    rmdir /s /q build
    echo Done.
    echo.
)

REM Activate ESP-IDF
echo Activating ESP-IDF 6.0...
call C:\esp\v6.0.2\esp-idf\export.bat
echo.

REM Check environment
echo Environment Check:
echo   IDF_PATH = %IDF_PATH%
python --version
echo.

REM Build
echo ========================================
echo Starting Build...
echo ========================================
idf.py build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo BUILD SUCCESSFUL!
    echo ========================================
    echo.
    if exist build\espure_parity_test.elf (
        echo Firmware: build\espure_parity_test.elf
        echo Binary:   build\espure_parity_test.bin
    )
) else (
    echo.
    echo ========================================
    echo BUILD FAILED!
    echo ========================================
    exit /b 1
)
