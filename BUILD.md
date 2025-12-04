# OpenFIDO Build Guide

This guide explains how to build OpenFIDO using the provided build scripts.

## Quick Start

### Windows (Batch Script)

```cmd
REM Basic build
build.bat

REM Clean release build
build.bat clean release

REM Build for nRF52 with BLE
build.bat nrf52 release
```



### Linux/macOS (Bash)

```bash
# Make script executable (first time only)
chmod +x build.sh

# Basic build
./build.sh

# Install dependencies and build
./build.sh -i

# Clean build
./build.sh -c -b Release
```

## Build Script Options

### Windows (build.bat)

| Option | Description |
|--------|-------------|
| `(none)` | Build in Debug mode |
| `release` | Build in Release mode |
| `debug` | Build in Debug mode (default) |
| `clean` | Clean build directory before building |
| `ble` | Enable BLE transport support |
| `esp32` | Build for ESP32 platform |
| `stm32` | Build for STM32 platform |
| `nrf52` | Build for nRF52 platform (BLE auto-enabled) |
| `help`, `-h`, `/?` | Show help message |

### Linux/macOS (build.sh)

| Option | Description | Default |
|--------|-------------|---------|
| `-p, --platform` | Target platform: ESP32, STM32, NRF52, NATIVE | NATIVE |
| `-b, --build-type` | Build type: Debug, Release | Debug |
| `-c, --clean` | Clean build directory before building | false |
| `-e, --enable-ble` | Enable BLE transport support | false |
| `-i, --install-deps` | Install required dependencies | false |
| `-v, --verbose` | Enable verbose build output | false |
| `-h, --help` | Show help message | - |

## Platform-Specific Builds

### Native Build (Development/Testing)

Build for your current platform (Windows/Linux/macOS) for development and testing.

**Windows:**
```cmd
build.bat debug
```

**Linux/macOS:**
```bash
./build.sh -p NATIVE -b Debug
```

### ESP32 Build

Build firmware for ESP32-S2/S3 microcontrollers.

**Prerequisites:**
- ESP-IDF v5.0 or later
- ESP32-S2 or ESP32-S3 board

**Windows:**
```cmd
build.bat esp32 release
```

**Linux/macOS:**
```bash
./build.sh -p ESP32 -b Release
```

**Flash:**
```bash
idf.py -p COM3 flash monitor  # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux
```

### STM32 Build

Build firmware for STM32 microcontrollers.

**Prerequisites:**
- ARM GCC toolchain (arm-none-eabi-gcc)
- ST-Link programmer

**Windows:**
```cmd
build.bat stm32 release
```

**Linux/macOS:**
```bash
./build.sh -p STM32 -b Release
```

**Flash:**
```bash
st-flash write build/openfido.bin 0x8000000
```

### nRF52 Build (with BLE)

Build firmware for Nordic nRF52 microcontrollers with BLE support.

**Prerequisites:**
- ARM GCC toolchain
- nRF5 SDK v17.0 or later
- nRF Command Line Tools (optional, for flashing)

**Windows:**
```cmd
build.bat nrf52 release
```

**Linux/macOS:**
```bash
./build.sh -p NRF52 -b Release -e
```

**Flash:**
```bash
nrfjprog --program build/openfido.hex --chiperase
nrfjprog --reset
```

## Dependencies

### Windows

The build script can automatically install dependencies using:
- Chocolatey (recommended)
- Scoop
- Winget

**Manual Installation:**
- CMake: https://cmake.org/download/
- MinGW: https://www.mingw-w64.org/
- mbedTLS: Build from source or use package manager

### Linux

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential libmbedtls-dev git
```

**Fedora/RHEL:**
```bash
sudo yum install -y cmake gcc gcc-c++ mbedtls-devel git
```

### macOS

**Homebrew:**
```bash
brew install cmake mbedtls
```



## Build Outputs

### Native Build
- **Binary:** `build/openfido` (Linux/macOS) or `build/openfido.exe` (Windows)

### ESP32 Build
- **Firmware:** `build/openfido.bin`
- **Bootloader:** `build/bootloader/bootloader.bin`
- **Partition Table:** `build/partition_table/partition-table.bin`

### STM32 Build
- **Binary:** `build/openfido.bin`
- **ELF:** `build/openfido.elf`

### nRF52 Build
- **Hex:** `build/openfido.hex`
- **Binary:** `build/openfido.bin`

## Troubleshooting

### CMake Configuration Failed

**Problem:** CMake cannot find required libraries (mbedTLS, etc.)

**Solution:**
```bash
# Windows - Install manually or via Chocolatey
choco install mbedtls

# Linux/macOS
./build.sh -i
```

### Compiler Not Found

**Problem:** GCC or ARM toolchain not in PATH

**Solution:**
- Windows: Install MinGW via Chocolatey or add to PATH
- Linux: `sudo apt-get install build-essential`
- macOS: Install Xcode Command Line Tools: `xcode-select --install`

### mbedTLS Not Found

**Problem:** mbedTLS library not installed

**Solution:**

**Windows:**
```cmd
REM Via Chocolatey
choco install mbedtls

REM Or build from source
git clone https://github.com/ARMmbed/mbedtls.git
cd mbedtls
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
mingw32-make install
```

**Linux:**
```bash
sudo apt-get install libmbedtls-dev
```

**macOS:**
```bash
brew install mbedtls
```



### ESP-IDF Not Found

**Problem:** `idf.py` command not found

**Solution:**
- Install ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
- Source the export script: `. $HOME/esp/esp-idf/export.sh` (Linux/macOS)
- Or run from ESP-IDF Command Prompt (Windows)

## Build Configuration

### Custom CMake Options

You can pass additional CMake options by modifying the build scripts or running CMake directly:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DPLATFORM=NATIVE \
      -DENABLE_BLE=ON \
      -DLOG_LEVEL=LOG_LEVEL_INFO \
      ..
make -j$(nproc)
```

### Available CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `PLATFORM` | Target platform (ESP32, STM32, NRF52, NATIVE) | ESP32 |
| `CMAKE_BUILD_TYPE` | Build type (Debug, Release) | Debug |
| `ENABLE_BLE` | Enable BLE transport | OFF (ON for NRF52) |
| `LOG_LEVEL` | Logging level | LOG_LEVEL_INFO |
| `ENABLE_COVERAGE` | Enable code coverage | OFF |

## Continuous Integration

The project includes GitHub Actions workflows for automated building:

- **CI Pipeline:** `.github/workflows/ci.yml`
- **Build Matrix:** Builds on multiple compilers (GCC, Clang)
- **Platform Builds:** ESP32, STM32, nRF52 via PlatformIO

View build status and logs in the GitHub Actions tab.

## Next Steps

After building:

1. **Flash to Hardware:** Flash to your target device
2. **Browser Testing:** Test with https://webauthn.io
3. **Development:** See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines

## Additional Resources

- [BUILDING.md](docs/BUILDING.md) - Detailed building instructions
- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - System architecture
- [README.md](README.md) - Project overview
