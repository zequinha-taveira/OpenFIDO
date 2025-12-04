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
 * @brief Check if connection is encrypted
 *
 * @return true if connection is encrypted, false otherwise
 */
bool ble_transport_is_encrypted(void);

/**
 * @brief Check if connection is paired
 *
 * @return true if connection is paired, false otherwise
 */
bool ble_transport_is_paired(void);

/**
 * @brief Get last activity timestamp
 *
 * Returns the timestamp of the last BLE activity (message received or sent).
 * Used for idle timeout detection and power management.
 *
 * @return Last activity timestamp in milliseconds
 */
uint64_t ble_transport_get_last_activity_ms(void);

/**
 * @brief Disconnect current connection
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_disconnect(void);

/* ========== Power Management ========== */

/**
 * @brief Check if transport should enter deep sleep
 *
 * Checks if the transport has been idle long enough to enter deep sleep mode.
 *
 * @return true if deep sleep should be entered, false otherwise
 */
bool ble_transport_should_enter_deep_sleep(void);

/**
 * @brief Update connection state and power management
 *
 * Should be called periodically from main loop to update connection
 * parameters and power management state based on activity.
 */
void ble_transport_update_power_state(void);

/**
 * @brief Enter deep sleep mode
 *
 * Puts the BLE transport into deep sleep mode to conserve power.
 * The device can be woken by button press or BLE connection request.
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_enter_deep_sleep(void);

/**
 * @brief Wake from deep sleep mode
 *
 * Wakes the BLE transport from deep sleep mode and resumes advertising.
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_wake_from_deep_sleep(void);

/* ========== Transport Abstraction Integration ========== */

/**
 * @brief Register BLE transport with transport abstraction layer
 *
 * Registers BLE transport operations with the transport abstraction,
 * allowing unified access to BLE alongside other transports.
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_transport_register(void);

/**
 * @brief BLE transport receive wrapper for transport abstraction
 *
 * This is a wrapper function that adapts BLE transport to the
 * transport abstraction receive interface. BLE uses a callback
 * model, so this function returns 0 (non-blocking).
 *
 * @param data Buffer to receive data (unused for BLE)
 * @param max_len Maximum length (unused for BLE)
 * @param cmd Command byte pointer (unused for BLE)
 * @return 0 (BLE uses callbacks, not polling)
 */
int ble_transport_receive_wrapper(uint8_t *data, size_t max_len, uint8_t *cmd);

/* ========== Error Codes ========== */

#define BLE_TRANSPORT_OK 0
#define BLE_TRANSPORT_ERROR -1
#define BLE_TRANSPORT_ERROR_INVALID_PARAM -2
#define BLE_TRANSPORT_ERROR_NOT_INITIALIZED -3
#define BLE_TRANSPORT_ERROR_NOT_CONNECTED -4
#define BLE_TRANSPORT_ERROR_BUSY -5
#define BLE_TRANSPORT_ERROR_NOT_ENCRYPTED -6
#define BLE_TRANSPORT_ERROR_NOT_PAIRED -7

#ifdef __cplusplus
}
#endif

#endif /* BLE_TRANSPORT_H */
