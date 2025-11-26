# Yubikey/Nitrokey Compatibility - Protocol Documentation

This document describes the proprietary protocols implemented for full Yubikey and Nitrokey compatibility.

## Overview

OpenFIDO now supports the following protocols in addition to FIDO2/CTAP2:

- **CCID** (Chip Card Interface Device) - Smart card reader protocol
- **PIV** (Personal Identity Verification) - NIST SP 800-73 smart card authentication
- **OpenPGP Card** - GnuPG smart card operations
- **Yubico Management** - Device configuration and management
- **OATH** (TOTP/HOTP) - One-time password generation
- **Yubikey OTP** - Yubico's proprietary OTP format

## CCID (Chip Card Interface Device)

### Purpose
CCID provides a standard USB protocol for smart card readers, allowing the device to function as a virtual smart card.

### Implementation Files
- `src/usb/usb_ccid.h` - CCID protocol definitions
- `src/usb/usb_ccid.c` - CCID implementation with APDU routing

### Key Features
- ISO 7816 APDU command/response handling
- Application registration and routing
- Multi-application support (PIV, OpenPGP, etc.)
- Standard ATR (Answer To Reset) generation

### Usage
Applications register with CCID using `usb_ccid_register_app()` with their AID (Application Identifier). CCID automatically routes SELECT commands and subsequent APDUs to the appropriate application handler.

## PIV (Personal Identity Verification)

### Purpose
PIV implements NIST SP 800-73-4 for government and enterprise smart card authentication.

### Implementation Files
- `src/piv/piv.h` - PIV protocol definitions
- `src/piv/piv.c` - PIV core implementation

### Key Features
- PIN verification and management (default PIN: `123456`)
- Certificate storage (up to 4 slots)
- Key generation (RSA, ECC)
- Digital signature operations
- X.509 certificate management

### Supported Operations
- `VERIFY` - PIN verification
- `CHANGE REFERENCE DATA` - Change PIN
- `GET DATA` - Retrieve certificates and data objects
- `PUT DATA` - Store certificates
- `GENERATE ASYMMETRIC KEY PAIR` - Generate keys
- `GENERAL AUTHENTICATE` - Sign/decrypt operations

### Testing
```bash
# Test with yubico-piv-tool
yubico-piv-tool -a status
yubico-piv-tool -a verify-pin -P 123456

# Test with pkcs11-tool
pkcs11-tool --module /usr/lib/opensc-pkcs11.so --list-slots
```

## OpenPGP Card

### Purpose
Implements OpenPGP Card specification v3.4 for GPG operations (encryption, signing, authentication).

### Implementation Files
- `src/openpgp/openpgp.h` - OpenPGP Card definitions
- `src/openpgp/openpgp.c` - OpenPGP Card implementation

### Key Features
- Three key slots (Signature, Decryption, Authentication)
- PIN management (User PIN: `123456`, Admin PIN: `12345678`)
- Key generation (RSA, ECC, EdDSA)
- Digital signature computation
- Decryption operations
- Cardholder data storage

### Supported Operations
- `VERIFY` - PIN verification
- `CHANGE REFERENCE DATA` - Change PIN
- `GET DATA` / `PUT DATA` - Data object access
- `GENERATE ASYMMETRIC KEY PAIR` - Generate keys
- `PSO:COMPUTE DIGITAL SIGNATURE` - Sign data
- `PSO:DECIPHER` - Decrypt data
- `INTERNAL AUTHENTICATE` - Authentication challenge

### Testing
```bash
# Test with gpg
gpg --card-status
gpg --card-edit

# Generate keys
gpg --card-edit
> admin
> generate
```

## Yubico Management Protocol

### Purpose
Proprietary protocol for configuring Yubikey-specific features and querying device information.

### Implementation Files
- `src/usb/ykman.h` - Yubico Management definitions
- `src/usb/ykman.c` - Management implementation

### Key Features
- Device information queries
- USB interface configuration
- Capability reporting
- Serial number access

### Supported Operations
- `GET DEVICE INFO` - Device capabilities and version
- `GET SERIAL` - Device serial number
- `SET MODE` - Enable/disable USB interfaces
- `READ/WRITE CONFIG` - Configuration management

### Testing
```bash
# Test with ykman
ykman info
ykman config usb --enable-all
```

## Configuration

All protocols can be enabled/disabled via `src/config.h`:

```c
#define CONFIG_ENABLE_PIV 1        // Enable PIV
#define CONFIG_ENABLE_OPENPGP 1    // Enable OpenPGP
#define CONFIG_ENABLE_YKMAN 1      // Enable Yubico Management
#define CONFIG_ENABLE_OATH 1       // Enable OATH
#define CONFIG_ENABLE_YKOTP 1      // Enable Yubikey OTP
```

## Architecture

### Protocol Stack

```
┌─────────────────────────────────────┐
│     Applications (gpg, ssh, etc)    │
└─────────────────────────────────────┘
                  │
┌─────────────────────────────────────┐
│      PC/SC / CCID Middleware        │
└─────────────────────────────────────┘
                  │
          ┌───────┴───────┐
          │  USB CCID     │
          └───────┬───────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
┌───▼───┐   ┌────▼────┐   ┌───▼────┐
│  PIV  │   │ OpenPGP │   │ YKMAN  │
└───────┘   └─────────┘   └────────┘
```

### Application Registration

Each protocol registers with CCID using its unique AID:

- **PIV**: `A0 00 00 03 08 00 00 10 00 01 00`
- **OpenPGP**: `D2 76 00 01 24 01`
- **YKMAN**: `A0 00 00 05 27 47 11 17`

## Security Considerations

### Default Credentials

**IMPORTANT**: Change default PINs in production!

- **PIV PIN**: `123456` (6 digits)
- **PIV PUK**: `12345678` (8 digits)
- **OpenPGP User PIN**: `123456`
- **OpenPGP Admin PIN**: `12345678`

### PIN Retry Limits

- PIV PIN: 3 retries
- PIV PUK: 3 retries
- OpenPGP User PIN: 3 retries
- OpenPGP Admin PIN: 3 retries

### Key Storage

All private keys are stored securely in the device's secure storage with encryption.

## Compatibility

### Tested Tools

- **PIV**: yubico-piv-tool, pkcs11-tool, OpenSC
- **OpenPGP**: GnuPG 2.x, Kleopatra
- **Management**: ykman (Yubikey Manager)

### Operating Systems

- Linux (via pcscd)
- Windows (via Windows Smart Card service)
- macOS (via built-in smart card support)

## Future Enhancements

- [ ] RSA key generation (currently placeholder)
- [ ] Hardware-backed key storage
- [ ] Additional ECC curves (Curve25519, brainpool)
- [ ] Touch-to-sign functionality
- [ ] Attestation support for PIV
- [ ] Key import/export for OpenPGP

## References

- [NIST SP 800-73-4](https://csrc.nist.gov/publications/detail/sp/800-73/4/final) - PIV Specification
- [OpenPGP Card Specification v3.4](https://gnupg.org/ftp/specs/OpenPGP-smart-card-application-3.4.pdf)
- [ISO/IEC 7816](https://www.iso.org/standard/54089.html) - Smart Card Standards
- [USB CCID Specification](https://www.usb.org/document-library/smart-card-ccid-rev-110)
