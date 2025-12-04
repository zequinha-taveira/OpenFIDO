# OpenFIDO Architecture

## Overview

OpenFIDO is a modular FIDO2 authenticator implementation designed for portability across multiple hardware platforms. The architecture follows a layered approach with clear separation of concerns.

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│              Client Device (Phone/Computer)              │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
┌───────▼────────┐       ┌───────▼────────┐
│  USB HID       │       │  BLE GATT      │
│  (CTAPHID)     │       │  (FIDO Service)│
└───────┬────────┘       └───────┬────────┘
        │                        │
        └────────────┬───────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              Transport Abstraction Layer                 │
│         (Unified CTAP2 Request/Response)                │
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
│  │   USB HID    │    BLE       │  Flash / Button / LED│ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│              Hardware Platform                           │
│         (ESP32 / STM32 / nRF52)                         │
└─────────────────────────────────────────────────────────┘
```

## Component Descriptions

### Transport Abstraction Layer (`src/transport/`)

Provides a unified interface for multiple transport mechanisms (USB and BLE). Responsible for:
- Transport registration and management
- Routing CTAP requests to the protocol layer
- Sending responses on the correct transport
- Handling transport switching and coordination

### USB HID Interface (`src/usb/`)

Handles USB Human Interface Device communication using the CTAPHID protocol. Responsible for:
- USB enumeration and configuration
- Packet framing and deframing
- Channel management
- Keepalive messages

### BLE Transport (`src/ble/`)

Implements Bluetooth Low Energy transport following the FIDO CTAP BLE specification. Consists of:

#### BLE Transport Manager (`ble_transport.c`)
- State machine management (IDLE, ADVERTISING, CONNECTED, PROCESSING)
- Connection state tracking and encryption enforcement
- Integration between fragmentation layer and GATT service
- Power management and idle timeout handling

#### BLE FIDO Service (`ble_fido_service.c`)
- FIDO GATT service (UUID 0xFFFD) implementation
- Control Point characteristic (write) for receiving CTAP commands
- Status characteristic (notify) for sending CTAP responses
- Service Revision and Bitfield characteristics (read) for capability discovery
- Pairing and encryption enforcement

#### Message Fragmentation (`ble_fragment.c`)
- Fragments large CTAP messages to fit BLE MTU size
- Reassembles incoming fragments into complete messages
- Handles INIT and CONT fragment types
- Sequence number tracking and validation

#### BLE HAL Interface (`src/hal/hal_ble.h`)
Platform-independent interface for BLE operations:
- BLE stack initialization and advertising
- GATT server operations
- Connection management
- Security and pairing operations
- Platform-specific implementations:
  - `hal/nrf52/hal_ble_nrf52.c`: Nordic SoftDevice implementation
  - `hal/hal_ble_stub.c`: Stub for platforms without BLE support

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
- **BLE**: Advertising, GATT operations, security
- **Flash**: Read/write/erase operations
- **Random**: Hardware RNG or entropy source
- **Button**: User presence detection
- **LED**: Status indication
- **Crypto**: Optional hardware acceleration
- **Time**: Timestamps and delays

Platform-specific implementations:
- `hal/esp32/`: ESP-IDF based implementation
- `hal/stm32/`: STM32 HAL based implementation
- `hal/nrf52/`: nRF5 SDK based implementation with BLE support

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
1. Browser → USB/BLE → CTAP2: GetAssertion request
2. CTAP2: Parse CBOR request
3. Storage: Find matching credentials for RP ID
4. HAL: Wait for user presence (button press)
5. Storage: Retrieve credential private key
6. Crypto: Sign client data hash
7. Storage: Increment signature counter
8. CTAP2: Encode CBOR response
9. USB/BLE → Browser: Assertion
```

### BLE Authentication Flow

```
1. Client → BLE: Connect to FIDO service (UUID 0xFFFD)
2. BLE Transport: Require pairing with LE Secure Connections
3. Client → Control Point: Write CTAP command fragments
4. BLE Fragment: Reassemble complete CTAP message
5. Transport Layer: Route to CTAP2 protocol layer
6. CTAP2: Process command (same as USB flow)
7. Transport Layer: Send response to BLE transport
8. BLE Fragment: Fragment response based on MTU
9. Status Characteristic → Client: Notify with response fragments
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

### BLE Security

- **Pairing Required**: All FIDO operations require BLE pairing
- **LE Secure Connections**: Uses numeric comparison for pairing
- **Encryption Enforcement**: CTAP commands rejected on unencrypted connections
- **Pairing Attempt Limiting**: Blocks pairing after 3 failed attempts for 60 seconds
- **Connection Isolation**: Each connection maintains independent security state

### Attestation

- Device-unique attestation key
- Self-signed or batch attestation certificate
- Prevents credential tracking across RPs

## BLE HAL Interface Requirements

Platforms that support BLE must implement the following HAL functions in `src/hal/hal_ble.h`:

### Initialization and Advertising
- `hal_ble_init()`: Initialize BLE stack with event callback
- `hal_ble_start_advertising()`: Begin advertising with FIDO service UUID
- `hal_ble_stop_advertising()`: Stop advertising
- `hal_ble_is_supported()`: Check if platform supports BLE

### GATT Operations
- `hal_ble_notify()`: Send notification on Status characteristic
- `hal_ble_disconnect()`: Disconnect from client

### Event Handling
The HAL must invoke the registered callback for these events:
- `HAL_BLE_EVENT_CONNECTED`: New client connection
- `HAL_BLE_EVENT_DISCONNECTED`: Client disconnected
- `HAL_BLE_EVENT_WRITE`: Data written to Control Point characteristic
- `HAL_BLE_EVENT_PAIRING_COMPLETE`: Pairing successful
- `HAL_BLE_EVENT_PAIRING_FAILED`: Pairing failed

### Platform-Specific Implementations

**nRF52** (`hal/nrf52/hal_ble_nrf52.c`):
- Uses Nordic SoftDevice BLE stack
- Implements FIDO GATT service with proper UUIDs
- Handles SoftDevice events and forwards to transport layer

**Other Platforms** (`hal/hal_ble_stub.c`):
- Returns `HAL_BLE_ERROR_NOT_SUPPORTED` for all operations
- Allows graceful degradation when BLE is not available

## Transport Coordination

The transport abstraction layer manages coexistence of USB and BLE:

### Simultaneous Transport Support
- Both USB and BLE can be active simultaneously
- Authenticator accepts commands from either transport
- Responses are sent on the transport that received the request

### Operation Serialization
- Only one CTAP operation can be in progress at a time
- Commands from other transport are queued or rejected during operations
- Prevents state corruption from concurrent operations

### Power Management
- BLE uses low-power advertising when not connected
- Connection intervals optimized for power efficiency
- Deep sleep after 5 minutes of inactivity on both transports

## Extension Points

### Adding New Platform

1. Create `src/hal/<platform>/hal_<platform>.c`
2. Implement all HAL interface functions
3. Optionally implement `src/hal/<platform>/hal_ble_<platform>.c` for BLE support
4. Add platform-specific build configuration
5. Test with conformance suite

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

### BLE-Specific Performance (nRF52840)

- **BLE Connection Setup**: ~500ms
  - Advertising discovery: ~200ms
  - Connection establishment: ~200ms
  - Pairing (first time): ~100ms

- **Message Fragmentation Overhead**: ~10-50ms
  - Depends on message size and MTU
  - Typical MTU: 23-247 bytes

- **BLE Latency**: ~30-100ms per operation
  - Connection interval dependent
  - Optimized for power vs. latency tradeoff

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
