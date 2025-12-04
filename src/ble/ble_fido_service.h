/**
 * @file ble_fido_service.h
 * @brief BLE FIDO GATT Service
 *
 * Implements the FIDO GATT service (UUID 0xFFFD) for BLE transport.
 * Provides characteristics for CTAP command/response communication.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef BLE_FIDO_SERVICE_H
#define BLE_FIDO_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== FIDO Service UUIDs ========== */

/**
 * @brief FIDO Service UUID (0xFFFD)
 */
#define BLE_FIDO_SERVICE_UUID 0xFFFD

/**
 * @brief FIDO Control Point characteristic UUID
 *
 * Used by client to send CTAP command fragments to authenticator.
 * Property: Write
 */
#define BLE_FIDO_CONTROL_POINT_UUID 0xF1D0FFF1

/**
 * @brief FIDO Status characteristic UUID
 *
 * Used by authenticator to send CTAP response fragments to client.
 * Property: Notify
 */
#define BLE_FIDO_STATUS_UUID 0xF1D0FFF2

/**
 * @brief FIDO Control Point Length characteristic UUID
 *
 * Indicates maximum length of Control Point writes.
 * Property: Read
 */
#define BLE_FIDO_CONTROL_POINT_LENGTH_UUID 0xF1D0FFF3

/**
 * @brief FIDO Service Revision characteristic UUID
 *
 * Indicates supported FIDO2 protocol version.
 * Property: Read
 */
#define BLE_FIDO_SERVICE_REVISION_UUID 0xF1D0FFF4

/**
 * @brief FIDO Service Revision Bitfield characteristic UUID
 *
 * Bitfield indicating supported protocol versions.
 * Property: Read
 */
#define BLE_FIDO_SERVICE_REVISION_BITFIELD_UUID 0xF1D0FFF5

/* ========== Service Revision Values ========== */

/**
 * @brief FIDO2 Revision 1.0
 */
#define BLE_FIDO_SERVICE_REVISION_1_0 0x80

/**
 * @brief FIDO2 Revision 1.1
 */
#define BLE_FIDO_SERVICE_REVISION_1_1 0x20

/**
 * @brief FIDO2 Revision 1.2
 */
#define BLE_FIDO_SERVICE_REVISION_1_2 0x40

/* ========== FIDO Service Callbacks ========== */

/**
 * @brief Control Point write callback
 *
 * Called when client writes to Control Point characteristic.
 *
 * @param conn_handle Connection handle
 * @param data Fragment data
 * @param len Fragment length
 */
typedef void (*ble_fido_control_point_write_cb_t)(uint16_t conn_handle, const uint8_t *data,
                                                  size_t len);

/**
 * @brief Status notification enabled callback
 *
 * Called when client enables notifications on Status characteristic.
 *
 * @param conn_handle Connection handle
 * @param enabled true if enabled, false if disabled
 */
typedef void (*ble_fido_status_notify_cb_t)(uint16_t conn_handle, bool enabled);

/**
 * @brief FIDO service callbacks
 */
typedef struct {
    ble_fido_control_point_write_cb_t on_control_point_write;
    ble_fido_status_notify_cb_t on_status_notify;
} ble_fido_service_callbacks_t;

/* ========== FIDO Service Functions ========== */

/**
 * @brief Initialize FIDO service
 *
 * Registers the FIDO GATT service and all characteristics with the BLE stack.
 *
 * @param callbacks Service callbacks
 * @return 0 on success, negative error code otherwise
 */
int ble_fido_service_init(const ble_fido_service_callbacks_t *callbacks);

/**
 * @brief Handle Control Point write
 *
 * Called by BLE HAL when Control Point characteristic is written.
 *
 * @param conn_handle Connection handle
 * @param data Fragment data
 * @param len Fragment length
 */
void ble_fido_service_on_control_point_write(uint16_t conn_handle, const uint8_t *data, size_t len);

/**
 * @brief Send Status notification
 *
 * Sends a CTAP response fragment via Status characteristic notification.
 *
 * @param conn_handle Connection handle
 * @param data Fragment data
 * @param len Fragment length
 * @return 0 on success, negative error code otherwise
 */
int ble_fido_service_send_status(uint16_t conn_handle, const uint8_t *data, size_t len);

/**
 * @brief Get Control Point characteristic handle
 *
 * @return Control Point characteristic handle
 */
uint16_t ble_fido_service_get_control_point_handle(void);

/**
 * @brief Get Status characteristic handle
 *
 * @return Status characteristic handle
 */
uint16_t ble_fido_service_get_status_handle(void);

/**
 * @brief Check if Status notifications are enabled
 *
 * @param conn_handle Connection handle
 * @return true if enabled, false otherwise
 */
bool ble_fido_service_is_status_notify_enabled(uint16_t conn_handle);

/**
 * @brief Check if connection is authorized for FIDO operations
 *
 * Verifies that the connection is paired and encrypted before allowing
 * access to FIDO characteristics.
 *
 * @param conn_handle Connection handle
 * @return true if authorized, false otherwise
 */
bool ble_fido_service_is_authorized(uint16_t conn_handle);

/**
 * @brief Set notification enabled state
 *
 * Called by BLE HAL when client enables/disables notifications.
 *
 * @param conn_handle Connection handle
 * @param enabled true if enabled, false if disabled
 */
void ble_fido_service_set_status_notify_enabled(uint16_t conn_handle, bool enabled);

/* ========== Error Codes ========== */

#define BLE_FIDO_SERVICE_OK 0
#define BLE_FIDO_SERVICE_ERROR -1
#define BLE_FIDO_SERVICE_ERROR_INVALID_PARAM -2
#define BLE_FIDO_SERVICE_ERROR_NOT_INITIALIZED -3
#define BLE_FIDO_SERVICE_ERROR_NOT_PAIRED -4

#ifdef __cplusplus
}
#endif

#endif /* BLE_FIDO_SERVICE_H */
