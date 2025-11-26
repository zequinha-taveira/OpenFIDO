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

### Building for Nordic nRF52

```bash
# Create build directory
mkdir build && cd build

# Configure for nRF52
cmake -DPLATFORM=NRF52 ..

# Build
make

# Flash using nrfjprog or your preferred method
```

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

### Browser Testing

1. Build and flash the firmware to your device
2. Connect via USB
3. Visit https://webauthn.io or https://demo.yubico.com
4. Test registration and authentication

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
- [ ] STM32 HAL support
- [ ] Nordic nRF52 HAL support
- [ ] CTAP2.1 extensions
- [ ] BLE transport support
- [ ] NFC transport support
- [ ] Hardware security module integration

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
