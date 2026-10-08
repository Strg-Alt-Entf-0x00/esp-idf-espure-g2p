@echo off
REM Verbose Rebuild Script - Shows all details
echo ========================================
echo ESP32 G2P Test - VERBOSE REBUILD
echo ========================================
echo.

REM Clean
if exist build (
    echo Cleaning old build...
    rmdir /s /q build
)

REM Activate ESP-IDF with full environment
echo Activating ESP-IDF 6.0...
call C:\esp\v6.0.2\esp-idf\export.bat

echo.
echo ========================================
echo Starting Verbose Build...
echo ========================================
idf.py -v build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo BUILD SUCCESSFUL!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo BUILD FAILED - Exit Code: %ERRORLEVEL%
    echo ========================================
    echo.
    echo Check the output above for errors.
    pause
    exit /b 1
)
