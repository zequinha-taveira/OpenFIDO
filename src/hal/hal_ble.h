/**
 * @file hal_ble.h
 * @brief BLE Hardware Abstraction Layer Interface
 *
 * This file defines the BLE HAL interface for Bluetooth Low Energy operations.
 * Platform-specific implementations must provide these functions to enable
 * BLE transport for FIDO2 authentication.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef HAL_BLE_H
#define HAL_BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== BLE HAL Return Codes ========== */

#define HAL_BLE_OK 0
#define HAL_BLE_ERROR -1
#define HAL_BLE_ERROR_NOT_SUPPORTED -2
#define HAL_BLE_ERROR_BUSY -3
#define HAL_BLE_ERROR_TIMEOUT -4
#define HAL_BLE_ERROR_NO_MEM -5
#define HAL_BLE_ERROR_INVALID_PARAM -6
#define HAL_BLE_ERROR_INVALID_STATE -7

/* ========== BLE Connection Handle ========== */

/**
 * @brief BLE connection handle type
 */
typedef uint16_t hal_ble_conn_handle_t;

/**
 * @brief Invalid connection handle constant
 */
#define HAL_BLE_CONN_HANDLE_INVALID 0xFFFF

/* ========== BLE Event Types ========== */

/**
 * @brief BLE event types
 */
typedef enum {
    HAL_BLE_EVENT_CONNECTED,          /**< BLE connection established */
    HAL_BLE_EVENT_DISCONNECTED,       /**< BLE connection terminated */
    HAL_BLE_EVENT_WRITE,              /**< Characteristic write received */
    HAL_BLE_EVENT_READ,               /**< Characteristic read requested */
    HAL_BLE_EVENT_NOTIFY_COMPLETE,    /**< Notification transmission complete */
    HAL_BLE_EVENT_NOTIFY_ENABLED,     /**< Client enabled notifications */
    HAL_BLE_EVENT_NOTIFY_DISABLED,    /**< Client disabled notifications */
    HAL_BLE_EVENT_PAIRING_REQUEST,    /**< Pairing request received */
    HAL_BLE_EVENT_PAIRING_COMPLETE,   /**< Pairing completed successfully */
    HAL_BLE_EVENT_PAIRING_FAILED,     /**< Pairing failed */
    HAL_BLE_EVENT_ENCRYPTION_CHANGED, /**< Connection encryption status changed */
    HAL_BLE_EVENT_MTU_CHANGED         /**< MTU size changed */
} hal_ble_event_type_t;

/* ========== BLE Event Structure ========== */

/**
 * @brief BLE event structure passed to callback
 */
typedef struct {
    hal_ble_event_type_t type;        /**< Event type */
    hal_ble_conn_handle_t conn_handle; /**< Connection handle */
    uint16_t char_handle;             /**< Characteristic handle (for read/write events) */
    const uint8_t *data;              /**< Event data pointer (for write events) */
    size_t data_len;                  /**< Event data length */
    uint16_t mtu;                     /**< MTU size (for MTU_CHANGED event) */
    bool encrypted;                   /**< Encryption status (for ENCRYPTION_CHANGED event) */
    int error_code;                   /**< Error code (for failure events) */
} hal_ble_event_t;

/* ========== BLE Event Callback ========== */

/**
 * @brief BLE event callback function type
 *
 * This callback is invoked by the BLE HAL when BLE events occur.
 * The callback is called from the BLE stack context and should
 * return quickly to avoid blocking BLE operations.
 *
 * @param event Pointer to event structure
 */
typedef void (*hal_ble_event_callback_t)(const hal_ble_event_t *event);

/* ========== BLE Security Configuration ========== */

/**
 * @brief BLE security mode
 */
typedef enum {
    HAL_BLE_SECURITY_MODE_OPEN,           /**< No security */
    HAL_BLE_SECURITY_MODE_ENCRYPTED,      /**< Encrypted connection */
    HAL_BLE_SECURITY_MODE_AUTHENTICATED   /**< Authenticated encrypted connection */
} hal_ble_security_mode_t;

/**
 * @brief BLE pairing method
 */
typedef enum {
    HAL_BLE_PAIRING_METHOD_JUST_WORKS,    /**< Just Works pairing */
    HAL_BLE_PAIRING_METHOD_NUMERIC,       /**< Numeric comparison */
    HAL_BLE_PAIRING_METHOD_PASSKEY        /**< Passkey entry */
} hal_ble_pairing_method_t;

/**
 * @brief BLE security configuration
 */
typedef struct {
    hal_ble_security_mode_t mode;         /**< Security mode */
    hal_ble_pairing_method_t pairing;     /**< Pairing method */
    bool bonding_enabled;                 /**< Enable bonding */
    bool mitm_protection;                 /**< Man-in-the-middle protection */
} hal_ble_security_config_t;

/* ========== BLE Advertising Configuration ========== */

/**
 * @brief BLE advertising parameters
 */
typedef struct {
    uint16_t interval_min_ms;             /**< Minimum advertising interval (ms) */
    uint16_t interval_max_ms;             /**< Maximum advertising interval (ms) */
    uint16_t timeout_s;                   /**< Advertising timeout (seconds, 0 = no timeout) */
    bool connectable;                     /**< Connectable advertising */
} hal_ble_adv_params_t;

/* ========== BLE Initialization and Control ========== */

/**
 * @brief Initialize BLE stack
 *
 * Initializes the platform-specific BLE stack and registers the event callback.
 * Must be called before any other BLE HAL functions.
 *
 * @param callback Event callback function
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_init(hal_ble_event_callback_t callback);

/**
 * @brief Deinitialize BLE stack
 *
 * Stops all BLE operations and releases resources.
 *
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_deinit(void);

/**
 * @brief Check if BLE is supported on this platform
 *
 * @return true if BLE is supported, false otherwise
 */
bool hal_ble_is_supported(void);

/* ========== BLE Advertising ========== */

/**
 * @brief Start BLE advertising
 *
 * Begins advertising with the specified parameters. The device becomes
 * discoverable and connectable (if configured).
 *
 * @param params Advertising parameters (NULL for default)
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_start_advertising(const hal_ble_adv_params_t *params);

/**
 * @brief Stop BLE advertising
 *
 * Stops advertising. The device is no longer discoverable.
 *
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_stop_advertising(void);

/**
 * @brief Check if currently advertising
 *
 * @return true if advertising, false otherwise
 */
bool hal_ble_is_advertising(void);

/* ========== BLE GATT Operations ========== */

/**
 * @brief Register GATT service
 *
 * Registers a GATT service with the specified UUID and characteristics.
 * Platform-specific implementation handles service registration.
 *
 * @param service_uuid 16-bit service UUID
 * @param service_handle Output parameter for service handle
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_gatt_register_service(uint16_t service_uuid, uint16_t *service_handle);

/**
 * @brief Add characteristic to service
 *
 * Adds a characteristic to a previously registered service.
 *
 * @param service_handle Service handle
 * @param char_uuid 32-bit characteristic UUID
 * @param properties Characteristic properties (read/write/notify flags)
 * @param max_len Maximum characteristic value length
 * @param char_handle Output parameter for characteristic handle
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_gatt_add_characteristic(uint16_t service_handle, uint32_t char_uuid,
                                     uint8_t properties, uint16_t max_len,
                                     uint16_t *char_handle);

/**
 * @brief Send notification
 *
 * Sends a notification to the connected client for the specified characteristic.
 *
 * @param conn_handle Connection handle
 * @param char_handle Characteristic handle
 * @param data Data to send
 * @param len Data length
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_notify(hal_ble_conn_handle_t conn_handle, uint16_t char_handle,
                   const uint8_t *data, size_t len);

/**
 * @brief Update characteristic value
 *
 * Updates the stored value of a characteristic. Used for read operations.
 *
 * @param char_handle Characteristic handle
 * @param data New value data
 * @param len Data length
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_gatt_set_value(uint16_t char_handle, const uint8_t *data, size_t len);

/* ========== BLE Connection Management ========== */

/**
 * @brief Disconnect from client
 *
 * Terminates the BLE connection with the specified client.
 *
 * @param conn_handle Connection handle
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_disconnect(hal_ble_conn_handle_t conn_handle);

/**
 * @brief Get connection MTU
 *
 * Returns the current MTU (Maximum Transmission Unit) for the connection.
 *
 * @param conn_handle Connection handle
 * @param mtu Output parameter for MTU size
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_get_mtu(hal_ble_conn_handle_t conn_handle, uint16_t *mtu);

/**
 * @brief Check if connection is encrypted
 *
 * @param conn_handle Connection handle
 * @param encrypted Output parameter for encryption status
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_is_encrypted(hal_ble_conn_handle_t conn_handle, bool *encrypted);

/**
 * @brief Update connection parameters
 *
 * Requests new connection parameters (interval, latency, timeout).
 *
 * @param conn_handle Connection handle
 * @param interval_min_ms Minimum connection interval (ms)
 * @param interval_max_ms Maximum connection interval (ms)
 * @param latency Slave latency
 * @param timeout_ms Supervision timeout (ms)
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_update_connection_params(hal_ble_conn_handle_t conn_handle,
                                      uint16_t interval_min_ms, uint16_t interval_max_ms,
                                      uint16_t latency, uint16_t timeout_ms);

/* ========== BLE Security Operations ========== */

/**
 * @brief Configure security settings
 *
 * Sets the security configuration for BLE connections.
 *
 * @param config Security configuration
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_set_security_config(const hal_ble_security_config_t *config);

/**
 * @brief Initiate pairing
 *
 * Initiates pairing with the connected client.
 *
 * @param conn_handle Connection handle
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_start_pairing(hal_ble_conn_handle_t conn_handle);

/**
 * @brief Confirm numeric comparison
 *
 * Confirms or rejects numeric comparison during pairing.
 *
 * @param conn_handle Connection handle
 * @param confirm true to confirm, false to reject
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_pairing_confirm(hal_ble_conn_handle_t conn_handle, bool confirm);

/**
 * @brief Delete bonding information
 *
 * Removes stored bonding information for a peer device.
 *
 * @param conn_handle Connection handle
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_delete_bond(hal_ble_conn_handle_t conn_handle);

/* ========== BLE Device Information ========== */

/**
 * @brief Set device name
 *
 * Sets the BLE device name used in advertising.
 *
 * @param name Device name string
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_set_device_name(const char *name);

/**
 * @brief Get BLE MAC address
 *
 * Returns the device's BLE MAC address.
 *
 * @param addr Output buffer for MAC address (6 bytes)
 * @return HAL_BLE_OK on success, error code otherwise
 */
int hal_ble_get_address(uint8_t addr[6]);

/* ========== GATT Characteristic Properties ========== */

/**
 * @brief GATT characteristic property flags
 */
#define HAL_BLE_GATT_PROP_READ          0x02  /**< Read property */
#define HAL_BLE_GATT_PROP_WRITE_NO_RSP  0x04  /**< Write without response */
#define HAL_BLE_GATT_PROP_WRITE         0x08  /**< Write with response */
#define HAL_BLE_GATT_PROP_NOTIFY        0x10  /**< Notify property */
#define HAL_BLE_GATT_PROP_INDICATE      0x20  /**< Indicate property */

#ifdef __cplusplus
}
#endif

#endif /* HAL_BLE_H */
