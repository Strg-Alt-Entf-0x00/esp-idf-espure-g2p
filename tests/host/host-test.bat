@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
echo =======================================================================
echo Host Unit Tests
echo Directory: %~dp0
echo =======================================================================

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake was not found in PATH.
    exit /b 1
)

where ctest >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ctest was not found in PATH.
    exit /b 1
)

if not exist "build" mkdir build

echo [1/3] Configuring CMake...
cmake -B build -S .
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b %errorlevel%
)

echo [2/3] Building test binaries...
cmake --build build --config Release
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b %errorlevel%
)

echo [3/3] Running tests via CTest...
ctest --test-dir build --output-on-failure -C Release
if errorlevel 1 (
    echo [ERROR] One or more host tests failed.
    exit /b %errorlevel%
)

echo =======================================================================
echo [SUCCESS] All host unit tests passed!
echo =======================================================================
exit /b 0
