/**
 * @file ble_transport.h
 * @brief BLE Transport Manager
 *
 * High-level BLE transport management that integrates the BLE HAL,
 * FIDO service, and fragmentation layer. Manages connection state,
 * CTAP message processing, and coordinates BLE operations.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Transport State ========== */

/**
 * @brief BLE transport state
 */
typedef enum {
    BLE_TRANSPORT_STATE_IDLE,        /**< Not initialized or stopped */
    BLE_TRANSPORT_STATE_ADVERTISING, /**< Advertising, waiting for connection */
    BLE_TRANSPORT_STATE_CONNECTED,   /**< Connected, ready for operations */
    BLE_TRANSPORT_STATE_PROCESSING   /**< Processing CTAP request */
} ble_transport_state_t;

/* ========== CTAP Callbacks ========== */

/**
 * @brief CTAP request callback
 *
 * Called when a complete CTAP request has been received and reassembled.
 * The callback should process the request and call ble_transport_send_response()
 * with the response.
 *
 * @param data CTAP request data
 * @param len Request length
 */
typedef void (*ble_transport_ctap_request_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief Connection state change callback
 *
 * Called when BLE connection state changes.
 *
 * @param connected true if connected, false if disconnected
 */
typedef void (*ble_transport_connection_cb_t)(bool connected);

/**
 * @brief Transport callbacks
 */
typedef struct {
    ble_transport_ctap_request_cb_t on_ctap_request;
    ble_transport_connection_cb_t on_connection_change;
} ble_transport_callbacks_t;

/* ========== Transport Management ========== */

/**
 * @brief Initialize BLE transport
 *
 * Initializes the BLE HAL, FIDO service, and fragmentation layer.
 * Must be called before any other BLE transport functions.
 *
 * @param callbacks Transport callbacks
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_init(const ble_transport_callbacks_t *callbacks);

/**
 * @brief Start BLE transport
 *
 * Starts BLE advertising and makes the device discoverable.
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_start(void);

/**
 * @brief Stop BLE transport
 *
 * Stops advertising and disconnects any active connections.
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_stop(void);

/**
 * @brief Get current transport state
 *
 * @return Current BLE transport state
 */
ble_transport_state_t ble_transport_get_state(void);

/**
 * @brief Check if BLE transport is connected
 *
 * @return true if connected, false otherwise
 */
bool ble_transport_is_connected(void);

/* ========== CTAP Message Processing ========== */

/**
 * @brief Process incoming CTAP request fragment
 *
 * Called by FIDO service when Control Point is written.
 * Handles fragment reassembly and invokes callback when complete.
 *
 * @param data Fragment data
 * @param len Fragment length
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_process_request(const uint8_t *data, size_t len);

/**
 * @brief Send CTAP response
 *
 * Fragments the response and sends it via Status characteristic notifications.
 *
 * @param data Response data
 * @param len Response length
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_send_response(const uint8_t *data, size_t len);

/* ========== Connection Management ========== */

/**
 * @brief Get current connection handle
 *
 * @return Connection handle, or HAL_BLE_CONN_HANDLE_INVALID if not connected
 */
uint16_t ble_transport_get_connection_handle(void);

/**
 * @brief Get connection MTU
 *
 * @return Current MTU size, or 0 if not connected
 */
uint16_t ble_transport_get_mtu(void);

/**
 * @brief Disconnect current connection
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_disconnect(void);

/* ========== Error Codes ========== */

#define BLE_TRANSPORT_OK 0
#define BLE_TRANSPORT_ERROR -1
#define BLE_TRANSPORT_ERROR_INVALID_PARAM -2
#define BLE_TRANSPORT_ERROR_NOT_INITIALIZED -3
#define BLE_TRANSPORT_ERROR_NOT_CONNECTED -4
#define BLE_TRANSPORT_ERROR_BUSY -5

#ifdef __cplusplus
}
#endif

#endif /* BLE_TRANSPORT_H */
