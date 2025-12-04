# Requirements Document

## Introduction

This document specifies the requirements for adding Bluetooth Low Energy (BLE) transport support to the OpenFIDO FIDO2 security key implementation. BLE transport enables wireless FIDO2 authentication, allowing users to authenticate without physical USB connection. The implementation will follow the FIDO Client to Authenticator Protocol (CTAP) specification for BLE transport, providing secure wireless communication between authenticators and client devices.

## Glossary

- **BLE**: Bluetooth Low Energy, a wireless communication protocol optimized for low power consumption
- **GATT**: Generic Attribute Profile, the BLE protocol for data exchange
- **Authenticator**: The OpenFIDO security key device that performs FIDO2 operations
- **Client**: The device (smartphone, computer) that communicates with the Authenticator
- **CTAP**: Client to Authenticator Protocol, the FIDO2 communication protocol
- **Service**: A collection of related BLE characteristics that provide specific functionality
- **Characteristic**: A data value in BLE that can be read, written, or notified
- **Pairing**: The process of establishing a secure BLE connection between devices
- **Advertisement**: BLE broadcast packets that allow devices to be discovered
- **MTU**: Maximum Transmission Unit, the maximum packet size for BLE data transfer

## Requirements

### Requirement 1

**User Story:** As a user, I want to authenticate wirelessly using BLE, so that I can use my security key without physical connection.

#### Acceptance Criteria

1. WHEN the Authenticator powers on THEN the system SHALL initialize the BLE stack and begin advertising
2. WHEN a Client scans for BLE devices THEN the system SHALL be discoverable with the FIDO service UUID
3. WHEN a Client connects to the Authenticator THEN the system SHALL establish a GATT connection
4. WHEN a Client sends a CTAP command over BLE THEN the system SHALL process the command and return the response
5. WHEN the BLE connection is idle for 30 seconds THEN the system SHALL maintain the connection without disconnecting

### Requirement 2

**User Story:** As a developer, I want the BLE transport to implement the FIDO CTAP BLE specification, so that the authenticator is compatible with standard FIDO2 clients.

#### Acceptance Criteria

1. WHEN the GATT server initializes THEN the system SHALL expose the FIDO service with UUID 0xFFFD
2. WHEN a Client reads the Service Revision characteristic THEN the system SHALL return the supported CTAP protocol version
3. WHEN a Client writes to the Control Point characteristic THEN the system SHALL accept CTAP command fragments
4. WHEN a CTAP response is ready THEN the system SHALL send response fragments via the Status characteristic notifications
5. WHEN a Client reads the Service Revision Bitfield characteristic THEN the system SHALL return the supported FIDO2 protocol versions

### Requirement 3

**User Story:** As a user, I want my BLE connection to be secure, so that my authentication credentials cannot be intercepted.

#### Acceptance Criteria

1. WHEN a Client attempts to connect THEN the system SHALL require BLE pairing with encryption
2. WHEN pairing is initiated THEN the system SHALL use LE Secure Connections with numeric comparison
3. WHEN the connection is established THEN the system SHALL enforce encrypted communication for all CTAP operations
4. WHEN an unpaired Client attempts to access FIDO characteristics THEN the system SHALL reject the access
5. WHEN pairing fails three times THEN the system SHALL temporarily block further pairing attempts for 60 seconds

### Requirement 4

**User Story:** As a developer, I want the BLE transport to handle message fragmentation, so that large CTAP messages can be transmitted over BLE's limited packet size.

#### Acceptance Criteria

1. WHEN a CTAP command exceeds the BLE MTU size THEN the system SHALL fragment the command into multiple packets
2. WHEN receiving command fragments THEN the system SHALL reassemble them into a complete CTAP command
3. WHEN sending a CTAP response that exceeds MTU size THEN the system SHALL fragment the response
4. WHEN all response fragments are sent THEN the system SHALL notify the Client that the response is complete
5. WHEN fragment reassembly fails due to missing packets THEN the system SHALL return a CTAP error response

### Requirement 5

**User Story:** As a user, I want the BLE transport to coexist with USB transport, so that I can use either connection method.

#### Acceptance Criteria

1. WHEN both USB and BLE are enabled THEN the system SHALL accept CTAP commands from either transport
2. WHEN a CTAP operation is in progress on one transport THEN the system SHALL queue or reject commands from the other transport
3. WHEN USB is connected THEN the system SHALL continue BLE advertising to allow simultaneous connections
4. WHEN switching between transports THEN the system SHALL maintain authenticator state consistency
5. WHEN both transports are idle THEN the system SHALL enter low-power mode

### Requirement 6

**User Story:** As a developer, I want the BLE implementation to use the HAL abstraction, so that it works across different hardware platforms.

#### Acceptance Criteria

1. WHEN the BLE transport initializes THEN the system SHALL use HAL functions for platform-specific BLE operations
2. WHEN the HAL does not support BLE THEN the system SHALL gracefully disable BLE transport
3. WHEN the platform provides hardware BLE acceleration THEN the system SHALL utilize it through the HAL
4. WHEN porting to a new platform THEN the system SHALL only require implementing HAL BLE functions
5. WHEN BLE operations fail at the HAL level THEN the system SHALL propagate appropriate error codes

### Requirement 7

**User Story:** As a user, I want visual feedback for BLE status, so that I know when the device is connected and active.

#### Acceptance Criteria

1. WHEN BLE is advertising THEN the system SHALL set the LED to slow blink
2. WHEN a BLE connection is established THEN the system SHALL set the LED to solid on
3. WHEN a CTAP operation is processing over BLE THEN the system SHALL set the LED to fast blink
4. WHEN BLE disconnects THEN the system SHALL return the LED to slow blink
5. WHEN BLE is disabled THEN the system SHALL turn off the BLE status LED indication

### Requirement 8

**User Story:** As a developer, I want comprehensive error handling for BLE operations, so that failures are handled gracefully.

#### Acceptance Criteria

1. WHEN a BLE connection is lost during a CTAP operation THEN the system SHALL abort the operation and clean up resources
2. WHEN the BLE stack fails to initialize THEN the system SHALL log the error and continue with USB-only mode
3. WHEN invalid data is received on BLE characteristics THEN the system SHALL return a CTAP error response
4. WHEN BLE buffer overflow occurs THEN the system SHALL reject new data and signal an error
5. WHEN the Client sends malformed CTAP fragments THEN the system SHALL reset the fragment buffer and return an error

### Requirement 9

**User Story:** As a user, I want the BLE transport to be power efficient, so that battery-powered operation is practical.

#### Acceptance Criteria

1. WHEN no BLE connection is active THEN the system SHALL use low-power advertising intervals
2. WHEN a BLE connection is idle THEN the system SHALL negotiate the longest acceptable connection interval
3. WHEN CTAP operations complete THEN the system SHALL return to low-power state within 1 second
4. WHEN the device is inactive for 5 minutes THEN the system SHALL enter deep sleep mode
5. WHEN in deep sleep mode THEN the system SHALL wake on button press or BLE connection request

### Requirement 10

**User Story:** As a developer, I want the BLE transport to be testable, so that I can verify correct operation.

#### Acceptance Criteria

1. WHEN running unit tests THEN the system SHALL provide mock BLE HAL functions
2. WHEN testing fragmentation THEN the system SHALL correctly handle messages of varying sizes
3. WHEN testing security THEN the system SHALL verify that unpaired access is rejected
4. WHEN testing error conditions THEN the system SHALL validate proper error responses
5. WHEN testing with real hardware THEN the system SHALL successfully complete FIDO2 conformance tests over BLE
