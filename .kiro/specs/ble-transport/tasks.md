# BLE Transport Implementation Plan

- [x] 1. Create BLE HAL interface and stub implementations





  - [x] 1.1 Define BLE HAL interface in `src/hal/hal_ble.h`


    - Define BLE event types, connection handles, and callback structures
    - Define functions for initialization, advertising, GATT operations, and security
    - Include error codes and status enums
    - _Requirements: 6.1, 6.2, 6.4_

  - [x] 1.2 Create stub BLE HAL implementation for platforms without BLE support


    - Implement `src/hal/hal_ble_stub.c` that returns `HAL_BLE_ERROR_NOT_SUPPORTED`
    - Ensure graceful degradation when BLE is not available
    - _Requirements: 6.2_


  - [x] 1.3 Implement nRF52 BLE HAL in `src/hal/nrf52/hal_ble_nrf52.c`

    - Initialize Nordic SoftDevice BLE stack
    - Implement advertising with FIDO service UUID
    - Implement GATT server operations
    - Implement pairing and encryption handlers
    - _Requirements: 1.1, 1.2, 3.1, 3.2, 3.3, 6.1, 6.3_

- [x] 2. Implement BLE message fragmentation layer


  - [x] 2.1 Create fragmentation module in `src/ble/ble_fragment.c` and `src/ble/ble_fragment.h`

    - Implement fragment buffer structure and initialization
    - Implement fragment assembly for incoming messages
    - Implement fragment creation for outgoing messages
    - Handle sequence numbers and fragment types (INIT/CONT)
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [ ]* 2.2 Write unit tests for fragmentation in `tests/test_ble_fragment.c`
    - Test single-packet messages (no fragmentation needed)
    - Test multi-packet message assembly
    - Test fragment creation with various MTU sizes
    - Test error handling for missing/out-of-order fragments
    - _Requirements: 4.1, 4.2, 4.3, 4.5, 10.2_

- [x] 3. Implement BLE FIDO GATT service




  - [x] 3.1 Create FIDO service module in `src/ble/ble_fido_service.c` and `src/ble/ble_fido_service.h`

    - Define FIDO service UUID (0xFFFD) and characteristic UUIDs
    - Implement service initialization and registration
    - Implement Control Point characteristic (write handler)
    - Implement Status characteristic (notification sender)
    - Implement Service Revision and Bitfield characteristics (read handlers)
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5_


  - [x] 3.2 Implement characteristic access control

    - Enforce pairing requirement for FIDO characteristics
    - Reject unpaired access attempts
    - _Requirements: 3.1, 3.3, 3.4_

- [x] 4. Implement BLE transport manager




  - [x] 4.1 Create transport manager in `src/ble/ble_transport.c` and `src/ble/ble_transport.h`

    - Implement state machine (IDLE, ADVERTISING, CONNECTED, PROCESSING)
    - Implement initialization and startup functions
    - Implement connection state tracking
    - Integrate fragmentation layer with GATT service
    - _Requirements: 1.1, 1.3, 1.4_

  - [x] 4.2 Implement CTAP request processing pipeline

    - Receive fragments from Control Point characteristic
    - Reassemble complete CTAP messages
    - Forward to CTAP2 protocol layer
    - _Requirements: 1.4, 4.2_

  - [x] 4.3 Implement CTAP response transmission pipeline

    - Receive CTAP responses from protocol layer
    - Fragment responses based on MTU
    - Send fragments via Status characteristic notifications
    - _Requirements: 1.4, 4.3, 4.4_


  - [x] 4.4 Implement connection management





    - Track connection state and encryption status
    - Handle connection and disconnection events
    - Manage connection intervals for power efficiency
    - _Requirements: 1.3, 1.5, 3.1, 3.3, 9.2_

- [x] 5. Implement BLE security and pairing





  - [x] 5.1 Implement pairing handler in BLE transport manager


    - Enforce LE Secure Connections with numeric comparison
    - Track pairing attempts and implement blocking after failures
    - Maintain pairing state per connection
    - _Requirements: 3.1, 3.2, 3.5_

  - [x] 5.2 Implement encryption enforcement


    - Verify encryption before processing CTAP commands
    - Reject operations on unencrypted connections
    - _Requirements: 3.3, 3.4_

  - [ ]* 5.3 Write unit tests for security enforcement in `tests/test_ble_security.c`
    - Test that unpaired access is rejected
    - Test pairing attempt blocking after failures
    - Test encryption requirement enforcement
    - _Requirements: 3.4, 3.5, 10.3_

- [x] 6. Implement transport abstraction layer




  - [x] 6.1 Create transport abstraction in `src/transport/transport.c` and `src/transport/transport.h`


    - Define transport type enum (USB, BLE)
    - Define transport operations structure
    - Implement transport registration
    - Implement unified send/receive interface
    - Track active transport
    - _Requirements: 5.1, 5.2, 5.4_

  - [x] 6.2 Refactor USB HID to use transport abstraction


    - Register USB transport with operations
    - Update main loop to use transport abstraction
    - _Requirements: 5.1, 5.3_

  - [x] 6.3 Register BLE transport with abstraction layer


    - Implement BLE transport operations
    - Handle transport switching and queuing
    - _Requirements: 5.1, 5.2_

- [x] 7. Integrate BLE transport with main application




  - [x] 7.1 Update `src/main.c` to initialize BLE transport


    - Check if BLE is supported via HAL
    - Initialize BLE transport if available
    - Start BLE advertising
    - Log BLE status
    - _Requirements: 1.1, 6.2, 8.2_

  - [x] 7.2 Update main loop to handle both USB and BLE transports

    - Poll both transports for incoming data
    - Route CTAP requests through transport abstraction
    - Send responses on correct transport
    - _Requirements: 5.1, 5.2, 5.3_

  - [x] 7.3 Implement transport coordination

    - Queue or reject commands when operation is in progress
    - Maintain authenticator state consistency across transports
    - _Requirements: 5.2, 5.4_

- [x] 8. Implement LED feedback for BLE status




  - [x] 8.1 Add BLE-specific LED patterns


    - Define BLE advertising pattern (slow blink)
    - Define BLE connected pattern (solid on)
    - Define BLE processing pattern (fast blink)
    - Update LED state based on BLE transport state
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5_

- [ ] 9. Implement power management
  - [ ] 9.1 Add power management to BLE transport
    - Use low-power advertising intervals when not connected
    - Negotiate long connection intervals when idle
    - Return to low-power state after operations
    - _Requirements: 9.1, 9.2, 9.3_

  - [ ] 9.2 Implement idle timeout and deep sleep
    - Track last activity timestamp
    - Enter deep sleep after 5 minutes of inactivity
    - Wake on button press or BLE connection
    - _Requirements: 9.4, 9.5_

- [ ] 10. Implement error handling
  - [ ] 10.1 Add comprehensive error handling to BLE transport
    - Handle connection loss during operations
    - Handle BLE stack initialization failures
    - Handle invalid data on characteristics
    - Handle buffer overflows
    - Handle malformed fragments
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

  - [ ] 10.2 Add error logging and recovery
    - Log all BLE errors with context
    - Clean up resources on errors
    - Return appropriate CTAP error codes
    - _Requirements: 6.5, 8.1, 8.2_

- [ ]* 11. Write integration tests
  - [ ]* 11.1 Create BLE transport integration tests in `tests/test_ble_integration.c`
    - Test complete CTAP command flow over BLE
    - Test fragmentation with real CTAP messages
    - Test transport switching between USB and BLE
    - Test error recovery scenarios
    - _Requirements: 10.1, 10.4_

- [ ] 12. Update build system and documentation
  - [ ] 12.1 Update CMakeLists.txt to include BLE source files
    - Add BLE HAL interface
    - Add BLE transport modules
    - Add transport abstraction
    - Add conditional compilation for BLE support
    - _Requirements: 6.1, 6.2_

  - [ ] 12.2 Update documentation
    - Document BLE transport architecture in `docs/ARCHITECTURE.md`
    - Document BLE HAL interface requirements
    - Add BLE usage instructions to README
    - _Requirements: 6.4_

- [ ] 13. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.
