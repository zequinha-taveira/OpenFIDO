# OpenFIDO Testing Guide

## Overview

This document describes the testing strategy and procedures for OpenFIDO.

## Test Types

### 1. Unit Tests

**Location**: `tests/`

**Purpose**: Test individual components in isolation

**Components Tested**:
- CBOR encoder/decoder
- Cryptographic operations
- Storage management
- Buffer utilities
- CTAP2 protocol functions

**Running Unit Tests**:
```bash
cd tests
mkdir build && cd build
cmake ..
make
./run_tests
```

**Coverage**:
```bash
cmake -DENABLE_COVERAGE=ON ..
make
./run_tests
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### 2. FIDO2 Conformance Tests

**Purpose**: Verify compliance with FIDO2 specifications

**Tools**:
- FIDO Alliance Conformance Test Tools
- Yubico fido2-tests

**Setup**:
```bash
# Install fido2-tests
git clone https://github.com/Yubico/python-fido2.git
cd python-fido2
pip install -e .

# Run tests
fido2-test
```

**Test Categories**:
- MakeCredential tests
- GetAssertion tests
- GetInfo tests
- ClientPIN tests
- Reset tests

### 3. Hardware-in-Loop Tests

**Purpose**: Test on real hardware

**Platforms**:
- ESP32-S3 DevKit
- STM32F4 Nucleo
- nRF52840 DK

**Test Procedure**:

#### ESP32
```bash
# Build and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# Verify USB enumeration
lsusb | grep FIDO

# Test with browser
# Navigate to https://webauthn.io
```

#### STM32
```bash
# Build
mkdir build && cd build
cmake -DPLATFORM=STM32 ..
make

# Flash
st-flash write openfido.bin 0x8000000

# Test
# Connect via USB and test with browser
```

#### nRF52
```bash
# Build
cd platforms/nrf52
make

# Flash
nrfjprog --program _build/openfido.hex --chiperase
nrfjprog --reset

# Test
# Connect via USB and test with browser
```

**Hardware Tests**:
- ✅ USB enumeration
- ✅ Button press detection
- ✅ LED indication
- ✅ Flash read/write
- ✅ RNG functionality
- ✅ Power consumption
- ✅ Timing accuracy

### 4. Browser Integration Tests

**Purpose**: Test with real web applications

**Test Sites**:
- https://webauthn.io - WebAuthn demo
- https://demo.yubico.com - Yubico demo
- https://webauthn.me - Microsoft demo
- https://github.com (actual registration)

**Test Scenarios**:

#### Registration Flow
1. Navigate to test site
2. Click "Register"
3. Verify LED blinks
4. Press button on device
5. Verify registration success
6. Check credential stored

#### Authentication Flow
1. Navigate to test site
2. Click "Authenticate"
3. Verify LED blinks
4. Press button on device
5. Verify authentication success
6. Check counter incremented

#### Error Handling
1. Test timeout (don't press button)
2. Test wrong RP ID
3. Test excluded credentials
4. Test PIN required scenarios

**Browsers to Test**:
- ✅ Chrome/Chromium
- ✅ Firefox
- ✅ Edge
- ✅ Safari (macOS/iOS)

## Test Matrix

| Test Type | CBOR | Crypto | Storage | CTAP2 | HAL | USB |
|-----------|------|--------|---------|-------|-----|-----|
| Unit | ✅ | ✅ | ✅ | ✅ | Mock | Mock |
| Integration | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Conformance | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Hardware | - | ✅ | ✅ | ✅ | ✅ | ✅ |
| Browser | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## Continuous Integration

**GitHub Actions Workflow** (`.github/workflows/test.yml`):

```yaml
name: Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: sudo apt-get install -y libmbedtls-dev
      - name: Build tests
        run: |
          cd tests
          mkdir build && cd build
          cmake ..
          make
      - name: Run tests
        run: cd tests/build && ./run_tests

  conformance-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup Python
        uses: actions/setup-python@v2
      - name: Install fido2-tests
        run: pip install fido2
      - name: Run conformance tests
        run: fido2-test
```

## Test Results

### Expected Results

**Unit Tests**: All tests should pass
```
=== Running CBOR Tests ===
PASS: test_cbor_encode_uint
PASS: test_cbor_encode_bytes
PASS: test_cbor_encode_map
...
=== CBOR Tests: 0 failures ===

=== Running Crypto Tests ===
PASS: test_crypto_sha256
PASS: test_crypto_ecdsa_keygen
...
=== Crypto Tests: 0 failures ===
```

**Conformance Tests**: Should pass FIDO2 Level 1
```
MakeCredential: PASS
GetAssertion: PASS
GetInfo: PASS
ClientPIN: PASS
Reset: PASS
```

**Browser Tests**: Should work on all major browsers
```
Chrome: ✅ Registration ✅ Authentication
Firefox: ✅ Registration ✅ Authentication
Edge: ✅ Registration ✅ Authentication
```

## Debugging Tests

### Enable Verbose Logging
```c
// In logger.h
#define LOG_LEVEL LOG_LEVEL_DEBUG
```

### USB Packet Capture
```bash
# Linux
sudo modprobe usbmon
sudo wireshark

# Filter: usb.device_address == X
```

### Flash Dump
```bash
# ESP32
esptool.py read_flash 0x0 0x400000 flash_dump.bin

# STM32
st-flash read flash_dump.bin 0x8000000 0x10000
```

## Performance Benchmarks

**Target Performance**:
- MakeCredential: < 500ms
- GetAssertion: < 200ms
- Button response: < 100ms
- USB latency: < 10ms

**Benchmark Tool**:
```bash
# Run performance tests
./run_tests --benchmark
```

## Security Testing

### Fuzzing
```bash
# AFL fuzzing
afl-gcc -o fuzz_cbor test_cbor.c ../src/fido2/cbor.c
afl-fuzz -i testcases -o findings ./fuzz_cbor @@
```

### Static Analysis
```bash
# Clang static analyzer
scan-build make

# Cppcheck
cppcheck --enable=all src/
```

### Penetration Testing
- Side-channel analysis
- Timing attacks
- Power analysis
- Fault injection

## Test Maintenance

- Update tests when adding features
- Maintain >80% code coverage
- Run tests before each commit
- Document test failures
- Keep conformance tests up to date

## Troubleshooting

**Test Failures**:
1. Check logs for error messages
2. Verify hardware connections
3. Update dependencies
4. Check FIDO2 spec compliance
5. Review recent code changes

**Common Issues**:
- USB enumeration fails → Check USB cable/port
- Button not detected → Verify GPIO configuration
- Flash errors → Check flash size/alignment
- Crypto failures → Verify mbedTLS installation
