@echo off
REM ============================================================================
REM OpenFIDO Build Script for Windows
REM ============================================================================
REM
REM Usage:
REM   build.bat                    - Build in Debug mode
REM   build.bat release            - Build in Release mode
REM   build.bat clean              - Clean build directory
REM   build.bat help               - Show this help
REM
REM ============================================================================

setlocal enabledelayedexpansion

REM Configuration
set BUILD_DIR=build
set BUILD_TYPE=Debug
set CLEAN_BUILD=0
set PLATFORM=NATIVE
set ENABLE_BLE=OFF

REM Parse command line arguments
:parse_args
if "%~1"=="" goto check_args
if /i "%~1"=="release" (
    set BUILD_TYPE=Release
    shift
    goto parse_args
)
if /i "%~1"=="debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%~1"=="clean" (
    set CLEAN_BUILD=1
    shift
    goto parse_args
)

if /i "%~1"=="ble" (
    set ENABLE_BLE=ON
    shift
    goto parse_args
)
if /i "%~1"=="esp32" (
    set PLATFORM=ESP32
    shift
    goto parse_args
)
if /i "%~1"=="stm32" (
    set PLATFORM=STM32
    shift
    goto parse_args
)
if /i "%~1"=="nrf52" (
    set PLATFORM=NRF52
    set ENABLE_BLE=ON
    shift
    goto parse_args
)
if /i "%~1"=="help" (
    goto show_help
)
if /i "%~1"=="-h" (
    goto show_help
)
if /i "%~1"=="/?" (
    goto show_help
)
echo Unknown option: %~1
goto show_help

:check_args

REM ============================================================================
REM Print header
REM ============================================================================
echo.
echo ========================================================================
echo  OpenFIDO Build Script
echo ========================================================================
echo  Platform:    %PLATFORM%
echo  Build Type:  %BUILD_TYPE%
echo  BLE Support: %ENABLE_BLE%
echo ========================================================================
echo.

REM ============================================================================
REM Check prerequisites
REM ============================================================================
echo [1/5] Checking prerequisites...
echo.

REM Check CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found!
    echo Please install CMake from: https://cmake.org/download/
    exit /b 1
)
for /f "tokens=*" %%i in ('cmake --version ^| findstr /r "cmake version"') do set CMAKE_VER=%%i
echo [OK] %CMAKE_VER%

REM Check GCC
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] GCC not found!
    echo Please install MinGW from: https://www.mingw-w64.org/
    exit /b 1
)
for /f "tokens=*" %%i in ('gcc --version ^| findstr /r "gcc"') do (
    echo [OK] %%i
    goto gcc_found
)
:gcc_found

REM Check Make
where mingw32-make >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] mingw32-make not found!
    echo Please install MinGW with make support
    exit /b 1
)
echo [OK] mingw32-make found

echo.
echo [OK] All prerequisites satisfied
echo.

REM ============================================================================
REM Clean build directory if requested
REM ============================================================================
if %CLEAN_BUILD%==1 (
    echo [2/4] Cleaning build directory...
    if exist %BUILD_DIR% (
        echo Removing %BUILD_DIR%...
        rmdir /s /q %BUILD_DIR%
    )
    echo [OK] Clean complete
    echo.
) else (
    echo [2/4] Skipping clean (use 'build.bat clean' to clean)
    echo.
)

REM ============================================================================
REM Configure CMake
REM ============================================================================
echo [3/4] Configuring CMake...
echo.

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

cmake -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
      -DPLATFORM=%PLATFORM% ^
      -DENABLE_BLE=%ENABLE_BLE% ^
      ..

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo [OK] CMake configuration complete
echo.

REM ============================================================================
REM Build
REM ============================================================================
echo [4/4] Building...
echo.

mingw32-make -j%NUMBER_OF_PROCESSORS%

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    exit /b 1
)

echo.
echo [OK] Build complete
echo.

REM Show binary size if it exists
if exist openfido.exe (
    for %%A in (openfido.exe) do (
        set size=%%~zA
        set /a size_kb=!size! / 1024
        echo Binary size: !size_kb! KB
    )
)

cd ..

REM ============================================================================
REM Build complete
REM ============================================================================
echo.
echo ========================================================================
echo  Build Complete!
echo ========================================================================
echo.
echo Build output: %BUILD_DIR%\
if %PLATFORM%==NATIVE (
    echo Executable:   %BUILD_DIR%\openfido.exe
)
echo.
echo Next steps:
if %PLATFORM%==NATIVE (
    echo   - Run: %BUILD_DIR%\openfido.exe
)
if %PLATFORM%==ESP32 (
    echo   - Flash: idf.py -p COM3 flash monitor
)
if %PLATFORM%==STM32 (
    echo   - Flash: st-flash write %BUILD_DIR%\openfido.bin 0x8000000
)
if %PLATFORM%==NRF52 (
    echo   - Flash: nrfjprog --program %BUILD_DIR%\openfido.hex --chiperase
)
echo.
exit /b 0

REM ============================================================================
REM Show help
REM ============================================================================
:show_help
echo.
echo OpenFIDO Build Script for Windows
echo.
echo Usage:
echo   build.bat [options]
echo.
echo Options:
echo   (none)          Build in Debug mode
echo   release         Build in Release mode
echo   debug           Build in Debug mode (default)
echo   clean           Clean build directory before building
echo   ble             Enable BLE transport support
echo   esp32           Build for ESP32 platform
echo   stm32           Build for STM32 platform
echo   nrf52           Build for nRF52 platform (BLE auto-enabled)
echo   help, -h, /?    Show this help message
echo.
echo Examples:
echo   build.bat                    - Basic debug build
echo   build.bat release            - Release build
echo   build.bat clean release      - Clean release build
echo   build.bat nrf52 release      - Build for nRF52 in release mode
echo   build.bat clean ble          - Clean build with BLE support
echo.
exit /b 0
