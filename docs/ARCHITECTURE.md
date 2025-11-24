# OpenFIDO Architecture

## Overview

OpenFIDO is a modular FIDO2 authenticator implementation designed for portability across multiple hardware platforms. The architecture follows a layered approach with clear separation of concerns.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    USB HID Interface                     │
│                  (CTAPHID Protocol)                      │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                  CTAP2 Protocol Layer                    │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ MakeCredential│ GetAssertion │  ClientPIN / Reset  │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                 Authenticator Core                       │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │   CBOR       │  Credential  │    Attestation       │ │
│  │  Encoding    │  Management  │                      │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              Cryptographic Layer                         │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │  ECDSA P-256 │  AES-256-GCM │   SHA-256 / HMAC    │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                Storage Layer                             │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ Credentials  │  PIN Data    │  Counters / Keys    │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│        Hardware Abstraction Layer (HAL)                  │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │   USB HID    │    Flash     │  Button / LED / RNG │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              Hardware Platform                           │
│         (ESP32 / STM32 / nRF52)                         │
└─────────────────────────────────────────────────────────┘
```

## Component Descriptions

### USB HID Interface (`src/usb/`)

Handles USB Human Interface Device communication using the CTAPHID protocol. Responsible for:
- USB enumeration and configuration
- Packet framing and deframing
- Channel management
- Keepalive messages

### CTAP2 Protocol Layer (`src/fido2/`)

Implements the FIDO2 Client-to-Authenticator Protocol version 2.x:
- **ctap2.c**: Main protocol dispatcher
- **cbor.c**: CBOR encoding/decoding for messages
- **authenticator.c**: Core authenticator logic

Supported commands:
- `authenticatorMakeCredential` (0x01)
- `authenticatorGetAssertion` (0x02)
- `authenticatorGetInfo` (0x04)
- `authenticatorClientPIN` (0x06)
- `authenticatorReset` (0x07)
- `authenticatorGetNextAssertion` (0x08)

### Cryptographic Layer (`src/crypto/`)

Provides cryptographic primitives using mbedTLS:
- **ECDSA P-256**: Key generation, signing, verification
- **AES-256-GCM**: Authenticated encryption for credential storage
- **SHA-256**: Hashing operations
- **HMAC-SHA256**: Message authentication
- **ECDH**: Key agreement for PIN protocol
- **HKDF**: Key derivation
- **CSPRNG**: Cryptographically secure random number generation

### Storage Layer (`src/storage/`)

Manages persistent storage of:
- **Credentials**: Encrypted credential private keys
- **Resident Keys**: Discoverable credentials with user information
- **PIN Data**: Hashed PIN and retry counters
- **Signature Counter**: Global monotonic counter
- **Attestation Key**: Device attestation private key and certificate

Storage is encrypted using AES-256-GCM with a device-unique key.

### Hardware Abstraction Layer (`src/hal/`)

Platform-independent interface for hardware operations:
- **USB**: Send/receive HID reports
- **Flash**: Read/write/erase operations
- **Random**: Hardware RNG or entropy source
- **Button**: User presence detection
- **LED**: Status indication
- **Crypto**: Optional hardware acceleration
- **Time**: Timestamps and delays

Platform-specific implementations:
- `hal/esp32/`: ESP-IDF based implementation
- `hal/stm32/`: STM32 HAL based implementation
- `hal/nrf52/`: nRF5 SDK based implementation

## Data Flow

### Registration (MakeCredential)

```
1. Browser → USB → CTAP2: MakeCredential request
2. CTAP2: Parse CBOR request
3. Authenticator: Validate RP ID, check excluded credentials
4. HAL: Wait for user presence (button press)
5. Crypto: Generate new ECDSA P-256 keypair
6. Storage: Store encrypted credential
7. Crypto: Create attestation signature
8. CTAP2: Encode CBOR response
9. USB → Browser: Attestation object
```

### Authentication (GetAssertion)

```
1. Browser → USB → CTAP2: GetAssertion request
2. CTAP2: Parse CBOR request
3. Storage: Find matching credentials for RP ID
4. HAL: Wait for user presence (button press)
5. Storage: Retrieve credential private key
6. Crypto: Sign client data hash
7. Storage: Increment signature counter
8. CTAP2: Encode CBOR response
9. USB → Browser: Assertion
```

## Security Considerations

### Credential Protection

- Credentials are encrypted with AES-256-GCM
- Encryption key derived from device-unique secret
- Each credential has unique IV
- Authentication tag prevents tampering

### PIN Protection

- PIN is hashed with SHA-256 before storage
- Limited retry counter (8 attempts)
- PIN required for sensitive operations
- PIN protocol uses ECDH key agreement

### User Presence

- Physical button press required for operations
- Timeout prevents automated attacks
- LED provides visual feedback

### Attestation

- Device-unique attestation key
- Self-signed or batch attestation certificate
- Prevents credential tracking across RPs

## Extension Points

### Adding New Platform

1. Create `src/hal/<platform>/hal_<platform>.c`
2. Implement all HAL interface functions
3. Add platform-specific build configuration
4. Test with conformance suite

### Adding CTAP2.1 Features

1. Update `ctap2.h` with new command codes
2. Implement command handler in `ctap2.c`
3. Update `authenticatorGetInfo` response
4. Add tests for new functionality

### Hardware Crypto Acceleration

1. Implement `hal_crypto_*` functions
2. Modify `crypto.c` to use HAL when available
3. Fallback to software implementation
4. Benchmark performance improvement

## Performance Characteristics

### Typical Operation Times (ESP32-S3 @ 240MHz)

- **MakeCredential**: ~200-300ms
  - Key generation: ~150ms
  - Storage write: ~50ms
  - Attestation: ~50ms

- **GetAssertion**: ~100-150ms
  - Credential lookup: ~20ms
  - Signature: ~80ms
  - Storage update: ~30ms

- **User Presence**: Variable (user-dependent)

### Memory Usage

- **RAM**: ~50KB
  - CTAP2 buffers: ~2KB
  - Crypto contexts: ~10KB
  - Stack: ~20KB
  - Heap: ~18KB

- **Flash**: ~200KB
  - Code: ~150KB
  - Credentials: ~50KB (50 credentials)

## Testing Strategy

### Unit Tests

- Individual component testing
- Mock HAL for platform independence
- CBOR encoding/decoding validation
- Cryptographic operation correctness

### Integration Tests

- Full CTAP2 command flows
- Storage persistence
- Error handling
- Edge cases

### Conformance Tests

- FIDO Alliance conformance tools
- WebAuthn.io testing
- Browser compatibility

### Hardware-in-Loop Tests

- Real hardware testing
- USB communication
- Flash endurance
- Button/LED functionality
