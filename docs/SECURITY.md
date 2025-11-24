# Security Considerations

## Overview

OpenFIDO implements a FIDO2 authenticator, which is a security-critical component. This document outlines the security architecture, threat model, and best practices.

## Threat Model

### Assumptions

**Trusted:**
- Hardware platform (no hardware backdoors)
- Cryptographic library (mbedTLS)
- User's physical security of device

**Untrusted:**
- Network communication
- Relying Party (website)
- Client platform (browser/OS)
- USB connection

### Threats

1. **Credential Theft**: Attacker extracts private keys
2. **Credential Forgery**: Attacker creates fake credentials
3. **Replay Attacks**: Attacker reuses old assertions
4. **PIN Brute Force**: Attacker guesses PIN
5. **Physical Attacks**: Attacker with device access
6. **Side Channel**: Timing or power analysis
7. **Malware**: Compromised client platform

## Security Features

### Cryptographic Protections

#### Key Generation
- **ECDSA P-256** keys generated using CSPRNG
- **Entropy Source**: Hardware RNG or platform entropy
- **Key Storage**: Never leaves device in plaintext

#### Credential Encryption
```
Encrypted Credential = AES-256-GCM(
    key: Device Master Key,
    iv: Random 12 bytes,
    plaintext: Private Key || Metadata,
    aad: RP ID Hash
)
```

**Properties:**
- Confidentiality: AES-256 encryption
- Integrity: GCM authentication tag
- Uniqueness: Random IV per credential
- Binding: AAD ties credential to RP

#### Signatures
- **Algorithm**: ECDSA with P-256 and SHA-256
- **Nonce**: Random k value per signature
- **Counter**: Monotonic signature counter prevents replay

### PIN Protection

#### PIN Storage
```
Stored PIN = SHA-256(UTF-8(PIN))
```

**Properties:**
- One-way hash prevents PIN recovery
- Constant-time comparison prevents timing attacks
- Limited retries (8 attempts) prevent brute force

#### PIN Protocol
- **ECDH Key Agreement**: Secure channel for PIN
- **HMAC Authentication**: Prevents tampering
- **Encrypted PIN**: Never sent in plaintext

### User Presence

- **Physical Button**: Required for all operations
- **Timeout**: 30-second window prevents automation
- **LED Indicator**: Visual feedback for user

### Attestation

#### Self-Attestation
- Device generates own attestation key
- Self-signed certificate
- No privacy concerns (no tracking)

#### Batch Attestation (Optional)
- Factory-programmed attestation key
- Shared across batch of devices
- Better privacy than individual attestation

### Signature Counter

- **Global Counter**: Monotonically increasing
- **Per-Credential**: Detects cloned authenticators
- **Persistent**: Survives power cycles
- **Atomic**: Increment and sign are atomic

## Implementation Security

### Memory Safety

**Buffer Overflow Prevention:**
```c
// Always check buffer sizes
if (len > MAX_BUFFER_SIZE) {
    return ERROR_BUFFER_TOO_SMALL;
}

// Use safe string functions
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';
```

**Integer Overflow Prevention:**
```c
// Check before arithmetic
if (a > SIZE_MAX - b) {
    return ERROR_OVERFLOW;
}
size_t result = a + b;
```

### Constant-Time Operations

**PIN Comparison:**
```c
// Constant-time comparison
int compare_constant_time(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}
```

**Avoid:**
```c
// WRONG: Early return leaks timing information
if (memcmp(pin_hash, stored_hash, 32) == 0) {
    return SUCCESS;
}
```

### Secure Erasure

**Zeroing Sensitive Data:**
```c
// Prevent compiler optimization
void secure_zero(void *ptr, size_t len)
{
    volatile uint8_t *p = ptr;
    while (len--) {
        *p++ = 0;
    }
}

// Use after sensitive operations
uint8_t private_key[32];
// ... use private_key ...
secure_zero(private_key, sizeof(private_key));
```

### Random Number Generation

**Requirements:**
- Cryptographically secure
- Sufficient entropy
- Properly seeded

**Implementation:**
```c
// Use hardware RNG if available
if (hal_random_generate(buffer, len) == HAL_OK) {
    return CRYPTO_OK;
}

// Fallback to mbedTLS CSPRNG
mbedtls_ctr_drbg_random(&ctr_drbg, buffer, len);
```

## Platform-Specific Security

### ESP32

**Secure Boot:**
- Enable secure boot in menuconfig
- Sign firmware images
- Prevents unauthorized firmware

**Flash Encryption:**
- Enable flash encryption
- Protects stored credentials
- Transparent to application

**Efuse:**
- Store device master key in efuse
- One-time programmable
- Hardware-protected

### STM32

**Read Protection:**
- Enable RDP Level 1 or 2
- Prevents flash readout
- Protects credentials

**Write Protection:**
- Protect bootloader sectors
- Prevents firmware tampering

**Secure Storage:**
- Use internal flash with RDP
- Consider external secure element

### Nordic nRF52

**Access Port Protection:**
- Enable APPROTECT
- Prevents debugger access
- Protects credentials

**Flash Protection:**
- Use ACL to protect regions
- Bootloader protection

## Best Practices

### Development

1. **Code Review**: All security code must be reviewed
2. **Static Analysis**: Use tools like clang-tidy
3. **Fuzzing**: Fuzz CBOR parser and protocol handlers
4. **Penetration Testing**: Regular security audits

### Deployment

1. **Secure Boot**: Enable on production devices
2. **Flash Encryption**: Encrypt credential storage
3. **Debug Disable**: Disable debug interfaces
4. **Unique Keys**: Each device has unique master key

### Updates

1. **Signed Firmware**: Cryptographically sign updates
2. **Rollback Protection**: Prevent downgrade attacks
3. **Secure Channel**: Use encrypted update mechanism

## Known Limitations

### Physical Access

An attacker with physical access may:
- Perform power analysis
- Attempt fault injection
- Extract keys from flash (if not encrypted)

**Mitigations:**
- Enable flash encryption
- Use secure element for key storage
- Implement tamper detection

### Side Channels

Potential side-channel leaks:
- Timing variations in crypto operations
- Power consumption patterns
- Electromagnetic emissions

**Mitigations:**
- Use constant-time implementations
- Consider hardware countermeasures
- Regular security audits

### Supply Chain

Risks:
- Compromised components
- Malicious firmware
- Backdoored tools

**Mitigations:**
- Verify component authenticity
- Reproducible builds
- Open-source transparency

## Reporting Security Issues

**DO NOT** create public issues for security vulnerabilities.

**Contact:** security@openfido.example.com

**Include:**
- Vulnerability description
- Proof of concept
- Potential impact
- Suggested mitigation

**Response:**
- Acknowledgment within 48 hours
- Assessment within 1 week
- Fix timeline communicated
- Credit in security advisory

## Security Checklist

Before deployment, verify:

- [ ] Flash encryption enabled
- [ ] Secure boot enabled
- [ ] Debug interfaces disabled
- [ ] Unique device keys
- [ ] PIN protection active
- [ ] Firmware signed
- [ ] Security audit completed
- [ ] Penetration test passed
- [ ] Conformance tests passed
- [ ] Documentation reviewed

## References

- [FIDO2 Specifications](https://fidoalliance.org/specifications/)
- [WebAuthn Security Considerations](https://www.w3.org/TR/webauthn/#sctn-security-considerations)
- [OWASP Embedded Security](https://owasp.org/www-project-embedded-application-security/)
- [mbedTLS Security Advisories](https://www.trustedfirmware.org/projects/mbed-tls/)
