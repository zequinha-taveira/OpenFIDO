# BLE Transport Design Document

## Overview

This design document specifies the implementation of Bluetooth Low Energy (BLE) transport for the OpenFIDO FIDO2 security key. The BLE transport will enable wireless FIDO2 authentication following the FIDO Client to Authenticator Protocol (CTAP) specification for BLE transport. The implementation will integrate seamlessly with the existing USB transport and CTAP2 protocol layer, providing users with flexible connectivity options.

The BLE transport implements the FIDO GATT service (UUID 0xFFFD) with characteristics for bidirectional communication, message fragmentation/reassembly, and secure pairing. The design maintains the existing layered architecture and uses the Hardware Abstraction Layer (HAL) for platform independence.

## Architecture

### System Architecture with BLE Transport

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
│  (MakeCredential, GetAssertion, ClientPIN, etc.)        │
└─────────────────────────────────────────────────────────┘
```

### BLE Transport Layer Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    BLE GATT Server                       │
│  ┌──────────────────────────────────────────────────┐  │
│  │         FIDO Service (UUID: 0xFFFD)              │  │
│  │  ┌────────────┬────────────┬──────────────────┐ │  │
│  │  │  Control   │   Status   │ Service Revision │ │  │
│  │  │   Point    │            │    & Bitfield    │ │  │
│  │  │  (Write)   │  (Notify)  │     (Read)       │ │  │
│  │  └────────────┴────────────┴──────────────────┘ │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│            BLE Transport Manager                         │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ Fragmentation│  Connection  │   Security/Pairing  │ │
│  │  & Assembly  │  Management  │                      │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                  BLE HAL Interface                       │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │ Advertising  │  GATT Ops    │   Security Ops      │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│         Platform BLE Stack (nRF52/ESP32/etc.)           │
└─────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. BLE HAL Interface (`src/hal/hal_ble.h`)

Platform-independent interface for BLE operations:

```c
/* BLE HAL Return Codes */
#define HAL_BLE_OK 0
#define HAL_BLE_ERROR -1
#define HAL_BLE_ERROR_NOT_SUPPORTED -2
#define HAL_BLE_ERROR_BUSY -3
#define HAL_BLE_ERROR_TIMEOUT -4
#define HAL_BLE_ERROR_NO_MEM -5

/* BLE Connection Handle */
typedef uint16_t hal_ble_conn_handle_t;
#define HAL_BLE_CONN_HANDLE_INVALID 0xFFFF

/* BLE Event Types */
typedef enum {
    HAL_BLE_EVENT_CONNECTED,
    HAL_BLE_EVENT_DISCONNECTED,
    HAL_BLE_EVENT_WRITE,
    HAL_BLE_EVENT_READ,
    HAL_BLE_EVENT_NOTIFY_COMPLETE,
    HAL_BLE_EVENT_PAIRING_COMPLETE,
    HAL_BLE_EVENT_PAIRING_FAILED
} hal_ble_event_type_t;

/* BLE Event Structure */
typedef struct {
    hal_ble_event_type_t type;
    hal_ble_conn_handle_t conn_handle;
    uint16_t char_handle;
    const uint8_t *data;
    size_t data_len;
} hal_ble_event_t;

/* BLE Event Callback */
typedef void (*hal_ble_event_callback_t)(const hal_ble_event_t *event);

/* Initialize BLE stack */
int hal_ble_init(hal_ble_event_callback_t callback);

/* Start advertising */
int hal_ble_start_advertising(void);

/* Stop advertising */
int hal_ble_stop_advertising(void);

/* Send notification */
int hal_ble_notify(hal_ble_conn_handle_t conn_handle, uint16_t char_handle,
                   const uint8_t *data, size_t len);

/* Disconnect */
int hal_ble_disconnect(hal_ble_conn_handle_t conn_handle);

/* Check if BLE is supported */
bool hal_ble_is_supported(void);
```

### 2. BLE Transport Manager (`src/ble/ble_transport.h`)

High-level BLE transport management:

```c
/* BLE Transport State */
typedef enum {
    BLE_TRANSPORT_STATE_IDLE,
    BLE_TRANSPORT_STATE_ADVERTISING,
    BLE_TRANSPORT_STATE_CONNECTED,
    BLE_TRANSPORT_STATE_PROCESSING
} ble_transport_state_t;

/* Initialize BLE transport */
int ble_transport_init(void);

/* Start BLE transport */
int ble_transport_start(void);

/* Stop BLE transport */
int ble_transport_stop(void);

/* Get current state */
ble_transport_state_t ble_transport_get_state(void);

/* Process incoming CTAP request (called by GATT handler) */
int ble_transport_process_request(const uint8_t *data, size_t len);

/* Send CTAP response (fragments and sends via notifications) */
int ble_transport_send_response(const uint8_t *data, size_t len);
```

### 3. BLE FIDO Service (`src/ble/ble_fido_service.h`)

FIDO GATT service implementation:

```c
/* FIDO Service UUID: 0xFFFD */
#define BLE_FIDO_SERVICE_UUID 0xFFFD

/* FIDO Characteristic UUIDs */
#define BLE_FIDO_CONTROL_POINT_UUID       0xF1D0FFF1
#define BLE_FIDO_STATUS_UUID              0xF1D0FFF2
#define BLE_FIDO_CONTROL_POINT_LENGTH_UUID 0xF1D0FFF3
#define BLE_FIDO_SERVICE_REVISION_UUID    0xF1D0FFF4
#define BLE_FIDO_SERVICE_REVISION_BITFIELD_UUID 0xF1D0FFF5

/* Service Revision Values */
#define BLE_FIDO_SERVICE_REVISION_1_0 0x80  /* FIDO2 Rev 1.0 */
#define BLE_FIDO_SERVICE_REVISION_1_1 0x20  /* FIDO2 Rev 1.1 */

/* Initialize FIDO service */
int ble_fido_service_init(void);

/* Handle Control Point write */
void ble_fido_service_on_control_point_write(const uint8_t *data, size_t len);

/* Send Status notification */
int ble_fido_service_send_status(const uint8_t *data, size_t len);
```

### 4. Message Fragmentation (`src/ble/ble_fragment.h`)

Handles fragmentation and reassembly of CTAP messages:

```c
/* Fragment Types */
#define BLE_FRAGMENT_TYPE_INIT 0x80
#define BLE_FRAGMENT_TYPE_CONT 0x00

/* Fragment Buffer */
typedef struct {
    uint8_t *buffer;
    size_t total_len;
    size_t received_len;
    uint8_t seq;
    bool in_progress;
} ble_fragment_buffer_t;

/* Initialize fragment buffer */
void ble_fragment_init(ble_fragment_buffer_t *frag);

/* Add fragment to buffer */
int ble_fragment_add(ble_fragment_buffer_t *frag, const uint8_t *data, size_t len);

/* Check if message is complete */
bool ble_fragment_is_complete(const ble_fragment_buffer_t *frag);

/* Get complete message */
int ble_fragment_get_message(const ble_fragment_buffer_t *frag, uint8_t **data, size_t *len);

/* Reset fragment buffer */
void ble_fragment_reset(ble_fragment_buffer_t *frag);

/* Fragment outgoing message */
int ble_fragment_create(const uint8_t *data, size_t len, uint8_t *fragments[], 
                        size_t fragment_sizes[], size_t *num_fragments, size_t mtu);
```

### 5. Transport Abstraction (`src/transport/transport.h`)

Unified interface for USB and BLE transports:

```c
/* Transport Type */
typedef enum {
    TRANSPORT_TYPE_USB,
    TRANSPORT_TYPE_BLE
} transport_type_t;

/* Transport Callbacks */
typedef struct {
    int (*send)(const uint8_t *data, size_t len);
    int (*receive)(uint8_t *data, size_t max_len, uint8_t *cmd);
    bool (*is_connected)(void);
} transport_ops_t;

/* Register transport */
int transport_register(transport_type_t type, const transport_ops_t *ops);

/* Send CTAP response on active transport */
int transport_send_response(const uint8_t *data, size_t len);

/* Get active transport type */
transport_type_t transport_get_active(void);
```

## Data Models

### BLE Connection State

```c
typedef struct {
    hal_ble_conn_handle_t conn_handle;
    bool is_paired;
    bool is_encrypted;
    uint16_t mtu;
    uint64_t last_activity_ms;
    uint8_t pairing_attempts;
    uint64_t pairing_block_until_ms;
} ble_connection_state_t;
```

### CTAP Message Fragment

```c
typedef struct {
    uint8_t cmd;           /* Fragment command byte */
    uint8_t seq;           /* Sequence number (0-127) */
    uint16_t total_len;    /* Total message length (init fragment only) */
    uint8_t data[512];     /* Fragment payload */
    size_t data_len;       /* Payload length */
} ctap_fragment_t;
```

### BLE FIDO Service Data

```c
typedef struct {
    uint16_t service_handle;
    uint16_t control_point_handle;
    uint16_t status_handle;
    uint16_t control_point_length_handle;
    uint16_t service_revision_handle;
    uint16_t service_revision_bitfield_handle;
    bool status_notifications_enabled;
} ble_fido_service_data_t;
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

