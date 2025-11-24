# Building OpenFIDO

This guide provides detailed instructions for building OpenFIDO on different hardware platforms.

## Prerequisites

### Common Requirements

- Git
- CMake 3.16 or later
- C compiler (GCC or Clang)
- Python 3.7 or later

### Platform-Specific Requirements

#### ESP32
- ESP-IDF v5.0 or later
- ESP32-S2 or ESP32-S3 board with USB support

#### STM32
- ARM GCC toolchain
- STM32CubeIDE (optional)
- ST-Link programmer

#### Nordic nRF52
- nRF5 SDK v17.0 or later
- ARM GCC toolchain
- nrfjprog or J-Link

## Building for ESP32

### Using ESP-IDF

1. **Install ESP-IDF**

```bash
# Follow official ESP-IDF installation guide
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
```

2. **Clone Repository**

```bash
git clone https://github.com/yourusername/OpenFIDO.git
cd OpenFIDO
```

3. **Set Target**

```bash
# For ESP32-S3
idf.py set-target esp32s3

# For ESP32-S2
idf.py set-target esp32s2
```

4. **Configure**

```bash
idf.py menuconfig
```

Navigate to:
- `Component config → TinyUSB → Enable TinyUSB`
- `Component config → mbedTLS → Enable mbedTLS`

5. **Build**

```bash
idf.py build
```

6. **Flash**

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Using PlatformIO

```bash
# Install PlatformIO
pip install platformio

# Build for ESP32-S3
pio run -e esp32s3

# Upload
pio run -e esp32s3 -t upload

# Monitor
pio device monitor
```

## Building for STM32

### Using CMake

1. **Install ARM Toolchain**

```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi

# macOS
brew install arm-none-eabi-gcc

# Windows
# Download from ARM website
```

2. **Install mbedTLS**

```bash
git clone https://github.com/ARMmbed/mbedtls.git
cd mbedtls
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-gcc-toolchain.cmake ..
make
sudo make install
```

3. **Build OpenFIDO**

```bash
cd OpenFIDO
mkdir build && cd build
cmake -DPLATFORM=STM32 -DCMAKE_TOOLCHAIN_FILE=../cmake/stm32-toolchain.cmake ..
make
```

4. **Flash**

```bash
# Using ST-Link
st-flash write openfido.bin 0x8000000

# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program openfido.elf verify reset exit"
```

### Using STM32CubeIDE

1. Import project into STM32CubeIDE
2. Configure build settings
3. Build project (Ctrl+B)
4. Debug/Run (F11/F5)

## Building for Nordic nRF52

### Using nRF5 SDK

1. **Install nRF5 SDK**

```bash
wget https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v17.x.x/nRF5_SDK_17.0.2_d674dde.zip
unzip nRF5_SDK_17.0.2_d674dde.zip -d ~/nrf5_sdk
```

2. **Install ARM Toolchain**

```bash
# Same as STM32 section
```

3. **Configure SDK Path**

Edit `Makefile.posix` or `Makefile.windows`:
```makefile
GNU_INSTALL_ROOT := /usr/local/gcc-arm-none-eabi-10-2020-q4-major/bin/
SDK_ROOT := ~/nrf5_sdk/
```

4. **Build**

```bash
cd OpenFIDO/platforms/nrf52
make
```

5. **Flash**

```bash
# Using nrfjprog
nrfjprog --program _build/openfido.hex --chiperase --verify
nrfjprog --reset

# Using J-Link
JLinkExe -device NRF52840_XXAA -if SWD -speed 4000
> loadfile _build/openfido.hex
> r
> g
> q
```

## Build Configurations

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Features:
- Debug symbols included
- Optimizations disabled (-O0)
- Verbose logging enabled
- Assertions enabled

### Release Build

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

Features:
- Optimizations enabled (-O2)
- Debug symbols stripped
- Minimal logging
- Assertions disabled

## Build Options

### CMake Options

```bash
# Set platform
-DPLATFORM=ESP32|STM32|NRF52

# Set log level
-DLOG_LEVEL=LOG_LEVEL_DEBUG|LOG_LEVEL_INFO|LOG_LEVEL_WARN|LOG_LEVEL_ERROR

# Enable tests
-DBUILD_TESTS=ON

# Enable examples
-DBUILD_EXAMPLES=ON
```

### Example

```bash
cmake -DPLATFORM=STM32 \
      -DCMAKE_BUILD_TYPE=Release \
      -DLOG_LEVEL=LOG_LEVEL_INFO \
      -DBUILD_TESTS=ON \
      ..
```

## Troubleshooting

### ESP32: USB Not Detected

- Ensure ESP32-S2/S3 variant (not ESP32 classic)
- Check USB cable supports data transfer
- Verify TinyUSB is enabled in menuconfig
- Try different USB port

### STM32: Flash Write Error

- Erase flash completely: `st-flash erase`
- Check ST-Link connection
- Verify correct target in OpenOCD config
- Update ST-Link firmware

### nRF52: Programming Failed

- Check J-Link/nrfjprog installation
- Verify SWD connections
- Erase chip: `nrfjprog --eraseall`
- Check power supply (3.3V)

### Build Errors

**Missing mbedTLS**
```bash
# Install mbedTLS development package
sudo apt-get install libmbedtls-dev
```

**Compiler not found**
```bash
# Verify toolchain installation
arm-none-eabi-gcc --version

# Add to PATH if needed
export PATH=$PATH:/path/to/gcc-arm-none-eabi/bin
```

## Testing the Build

### Quick Test

```bash
# Connect device via USB
# Device should enumerate as FIDO2 authenticator

# On Linux
lsusb | grep FIDO

# On macOS
system_profiler SPUSBDataType | grep FIDO

# On Windows
# Check Device Manager → Human Interface Devices
```

### Browser Test

1. Navigate to https://webauthn.io
2. Click "Register"
3. Device LED should blink
4. Press button on device
5. Registration should complete

### Conformance Test

```bash
# Clone FIDO2 test tools
git clone https://github.com/fido-alliance/conformance-test-tools-resources.git

# Run tests
cd conformance-test-tools-resources
python run_tests.py
```

## Next Steps

- Read [ARCHITECTURE.md](ARCHITECTURE.md) for system design
- See [HARDWARE.md](HARDWARE.md) for hardware setup
- Check [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines
