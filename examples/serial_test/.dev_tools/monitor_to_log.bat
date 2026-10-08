@echo off
REM ESP32-P4 Monitor Output to Log File
REM Project: esp-idf-espure Scientific Verification
REM Device: ESP32-P4 Rev 1.3 on COM4
REM
REM This script captures monitor output to a timestamped log file
REM Prevents context loss from large outputs and bootloops

echo ============================================================
echo ESP32-P4 Monitor to Log File
echo ============================================================

REM Set ESP-IDF environment
echo Setting up ESP-IDF 6.0.2 environment...
call C:\esp\v6.0.2\esp-idf\export.bat
if errorlevel 1 (
    echo ERROR: Failed to setup ESP-IDF environment
    exit /b 1
)

REM Create log directory if it doesn't exist
if not exist logs mkdir logs

REM Generate timestamped log filename
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set timestamp=%datetime:~0,8%_%datetime:~8,6%
set logfile=logs\monitor_%timestamp%.txt

echo.
echo Monitoring COM4...
echo Output will be saved to: %logfile%
echo Press Ctrl+] to stop monitoring
echo.

REM Monitor and save to log file
idf.py -p COM4 monitor > %logfile% 2>&1

echo.
echo ============================================================
echo Monitor session ended
echo Log saved to: %logfile%
echo ============================================================
