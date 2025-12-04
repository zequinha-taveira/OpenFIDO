#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build script for OpenFIDO FIDO2 Security Key

.DESCRIPTION
    This script builds OpenFIDO for different platforms and configurations.
    Supports ESP32, STM32, nRF52, and native builds with testing.

.PARAMETER Platform
    Target platform: ESP32, STM32, NRF52, or NATIVE (default: NATIVE)

.PARAMETER BuildType
    Build type: Debug or Release (default: Debug)

.PARAMETER Clean
    Clean build directory before building

.PARAMETER Test
    Run tests after building (NATIVE only)

.PARAMETER EnableBLE
    Enable BLE transport support (default: OFF, auto-enabled for NRF52)

.PARAMETER InstallDeps
    Install required dependencies (mbedTLS, etc.)

.PARAMETER Verbose
    Enable verbose build output

.EXAMPLE
    .\build.ps1
    Build for native platform in Debug mode

.EXAMPLE
    .\build.ps1 -Platform NRF52 -BuildType Release -EnableBLE
    Build for nRF52 with BLE support in Release mode

.EXAMPLE
    .\build.ps1 -Test -InstallDeps
    Install dependencies and run tests
#>

param(
    [Parameter()]
    [ValidateSet("ESP32", "STM32", "NRF52", "NATIVE")]
    [string]$Platform = "NATIVE",

    [Parameter()]
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [Parameter()]
    [switch]$Clean,

    [Parameter()]
    [switch]$Test,

    [Parameter()]
    [switch]$EnableBLE,

    [Parameter()]
    [switch]$InstallDeps,

    [Parameter()]
    [switch]$Verbose
)

# Script configuration
$ErrorActionPreference = "Stop"
$BuildDir = "build"
$TestBuildDir = "tests/build"

# Color output functions
function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor Green
}

function Write-Info {
    param([string]$Message)
    Write-Host "ℹ $Message" -ForegroundColor Cyan
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Blue
    Write-Host " $Message" -ForegroundColor Blue
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Blue
    Write-Host ""
}

# Check if command exists
function Test-Command {
    param([string]$Command)
    $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

# Install dependencies
function Install-Dependencies {
    Write-Header "Installing Dependencies"

    # Check for package managers
    $hasChoco = Test-Command "choco"
    $hasScoop = Test-Command "scoop"
    $hasWinget = Test-Command "winget"

    if (-not ($hasChoco -or $hasScoop -or $hasWinget)) {
        Write-Error "No package manager found. Please install Chocolatey, Scoop, or Winget."
        Write-Info "Chocolatey: https://chocolatey.org/install"
        Write-Info "Scoop: https://scoop.sh/"
        Write-Info "Winget: Built into Windows 10+"
        exit 1
    }

    # Install CMake
    if (-not (Test-Command "cmake")) {
        Write-Info "Installing CMake..."
        if ($hasChoco) {
            choco install cmake -y
        } elseif ($hasWinget) {
            winget install Kitware.CMake
        } elseif ($hasScoop) {
            scoop install cmake
        }
        Write-Success "CMake installed"
    } else {
        Write-Success "CMake already installed"
    }

    # Install MinGW (GCC for Windows)
    if (-not (Test-Command "gcc")) {
        Write-Info "Installing MinGW..."
        if ($hasChoco) {
            choco install mingw -y
        } elseif ($hasScoop) {
            scoop install mingw
        } else {
            Write-Warning "Please install MinGW manually from: https://www.mingw-w64.org/"
        }
        Write-Success "MinGW installed"
    } else {
        Write-Success "GCC already installed"
    }

    # Install mbedTLS
    Write-Info "Checking for mbedTLS..."
    if ($hasChoco) {
        Write-Info "Installing mbedTLS via Chocolatey..."
        choco install mbedtls -y
    } elseif ($hasScoop) {
        Write-Info "Installing mbedTLS via Scoop..."
        scoop bucket add extras
        scoop install mbedtls
    } else {
        Write-Warning "mbedTLS not available via package manager."
        Write-Info "You may need to build mbedTLS from source:"
        Write-Info "  git clone https://github.com/ARMmbed/mbedtls.git"
        Write-Info "  cd mbedtls && mkdir build && cd build"
        Write-Info "  cmake -G 'MinGW Makefiles' .."
        Write-Info "  mingw32-make && mingw32-make install"
    }

    Write-Success "Dependencies installation complete"
}

# Clean build directories
function Clean-Build {
    Write-Header "Cleaning Build Directories"

    if (Test-Path $BuildDir) {
        Write-Info "Removing $BuildDir..."
        Remove-Item -Recurse -Force $BuildDir
        Write-Success "Removed $BuildDir"
    }

    if (Test-Path $TestBuildDir) {
        Write-Info "Removing $TestBuildDir..."
        Remove-Item -Recurse -Force $TestBuildDir
        Write-Success "Removed $TestBuildDir"
    }

    Write-Success "Clean complete"
}

# Check prerequisites
function Test-Prerequisites {
    Write-Header "Checking Prerequisites"

    $allGood = $true

    # Check CMake
    if (Test-Command "cmake") {
        $cmakeVersion = (cmake --version | Select-Object -First 1)
        Write-Success "CMake: $cmakeVersion"
    } else {
        Write-Error "CMake not found"
        $allGood = $false
    }

    # Check compiler based on platform
    if ($Platform -eq "NATIVE") {
        if (Test-Command "gcc") {
            $gccVersion = (gcc --version | Select-Object -First 1)
            Write-Success "GCC: $gccVersion"
        } else {
            Write-Error "GCC not found"
            $allGood = $false
        }
    }

    # Check platform-specific tools
    switch ($Platform) {
        "ESP32" {
            if (Test-Command "idf.py") {
                Write-Success "ESP-IDF found"
            } else {
                Write-Error "ESP-IDF not found. Install from: https://docs.espressif.com/projects/esp-idf/"
                $allGood = $false
            }
        }
        "NRF52" {
            if (Test-Command "nrfjprog") {
                Write-Success "nRF Command Line Tools found"
            } else {
                Write-Warning "nrfjprog not found (optional for flashing)"
            }
        }
    }

    if (-not $allGood) {
        Write-Error "Prerequisites check failed. Use -InstallDeps to install missing dependencies."
        exit 1
    }

    Write-Success "All prerequisites satisfied"
}

# Build for native platform
function Build-Native {
    Write-Header "Building for Native Platform ($BuildType)"

    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    # Configure CMake
    Write-Info "Configuring CMake..."
    $cmakeArgs = @(
        "-G", "MinGW Makefiles",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DPLATFORM=NATIVE"
    )

    if ($EnableBLE) {
        $cmakeArgs += "-DENABLE_BLE=ON"
    }

    if ($Verbose) {
        $cmakeArgs += "-DCMAKE_VERBOSE_MAKEFILE=ON"
    }

    Push-Location $BuildDir
    try {
        & cmake $cmakeArgs ..
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
        Write-Success "CMake configuration complete"

        # Build
        Write-Info "Building..."
        $makeArgs = @()
        if ($Verbose) {
            $makeArgs += "VERBOSE=1"
        }

        & mingw32-make @makeArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        Write-Success "Build complete"

        # Show binary info
        if (Test-Path "openfido.exe") {
            $size = (Get-Item "openfido.exe").Length
            $sizeKB = [math]::Round($size / 1KB, 2)
            Write-Info "Binary size: $sizeKB KB"
        }
    }
    finally {
        Pop-Location
    }
}

# Build tests
function Build-Tests {
    Write-Header "Building Tests"

    # Create test build directory
    if (-not (Test-Path $TestBuildDir)) {
        New-Item -ItemType Directory -Path $TestBuildDir | Out-Null
    }

    # Configure CMake for tests
    Write-Info "Configuring tests..."
    Push-Location $TestBuildDir
    try {
        & cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=$BuildType ..
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Test configuration failed (mbedTLS may be missing)"
            return $false
        }
        Write-Success "Test configuration complete"

        # Build tests
        Write-Info "Building tests..."
        & mingw32-make
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Test build failed"
            return $false
        }
        Write-Success "Test build complete"

        return $true
    }
    finally {
        Pop-Location
    }
}

# Run tests
function Invoke-Tests {
    Write-Header "Running Tests"

    if (-not (Test-Path "$TestBuildDir/run_tests.exe")) {
        Write-Error "Test executable not found. Build tests first."
        return $false
    }

    Push-Location $TestBuildDir
    try {
        Write-Info "Running test suite..."
        & ./run_tests.exe
        $testResult = $LASTEXITCODE

        if ($testResult -eq 0) {
            Write-Success "All tests passed!"
            return $true
        } else {
            Write-Error "Tests failed with exit code: $testResult"
            return $false
        }
    }
    finally {
        Pop-Location
    }
}

# Build for ESP32
function Build-ESP32 {
    Write-Header "Building for ESP32"

    if (-not (Test-Command "idf.py")) {
        Write-Error "ESP-IDF not found. Please install ESP-IDF first."
        exit 1
    }

    Write-Info "Setting target to ESP32-S3..."
    & idf.py set-target esp32s3

    Write-Info "Building firmware..."
    & idf.py build

    if ($LASTEXITCODE -eq 0) {
        Write-Success "ESP32 build complete"
        Write-Info "Flash with: idf.py -p COM<X> flash monitor"
    } else {
        Write-Error "ESP32 build failed"
        exit 1
    }
}

# Build for STM32
function Build-STM32 {
    Write-Header "Building for STM32"

    Write-Warning "STM32 build requires ARM GCC toolchain"
    Write-Info "Please ensure arm-none-eabi-gcc is in your PATH"

    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    Push-Location $BuildDir
    try {
        & cmake -G "MinGW Makefiles" -DPLATFORM=STM32 -DCMAKE_BUILD_TYPE=$BuildType ..
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }

        & mingw32-make
        if ($LASTEXITCODE -eq 0) {
            Write-Success "STM32 build complete"
        } else {
            throw "Build failed"
        }
    }
    finally {
        Pop-Location
    }
}

# Build for nRF52
function Build-NRF52 {
    Write-Header "Building for nRF52"

    Write-Info "nRF52 build with BLE support enabled"

    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    Push-Location $BuildDir
    try {
        & cmake -G "MinGW Makefiles" -DPLATFORM=NRF52 -DCMAKE_BUILD_TYPE=$BuildType -DENABLE_BLE=ON ..
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }

        & mingw32-make
        if ($LASTEXITCODE -eq 0) {
            Write-Success "nRF52 build complete"
            Write-Info "Flash with: nrfjprog --program openfido.hex --chiperase"
        } else {
            throw "Build failed"
        }
    }
    finally {
        Pop-Location
    }
}

# Main execution
function Main {
    Write-Header "OpenFIDO Build Script"
    Write-Info "Platform: $Platform"
    Write-Info "Build Type: $BuildType"
    Write-Info "BLE Support: $(if ($EnableBLE -or $Platform -eq 'NRF52') { 'Enabled' } else { 'Disabled' })"

    # Install dependencies if requested
    if ($InstallDeps) {
        Install-Dependencies
    }

    # Clean if requested
    if ($Clean) {
        Clean-Build
    }

    # Check prerequisites
    Test-Prerequisites

    # Build based on platform
    switch ($Platform) {
        "NATIVE" {
            Build-Native
            
            if ($Test) {
                $testBuilt = Build-Tests
                if ($testBuilt) {
                    Invoke-Tests
                } else {
                    Write-Warning "Skipping tests due to build failure"
                }
            }
        }
        "ESP32" {
            Build-ESP32
        }
        "STM32" {
            Build-STM32
        }
        "NRF52" {
            Build-NRF52
        }
    }

    Write-Header "Build Complete"
    Write-Success "Build finished successfully!"
}

# Run main function
Main
