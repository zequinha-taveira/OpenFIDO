/**
 * @file transport.h
 * @brief Transport Abstraction Layer
 *
 * Provides a unified interface for USB and BLE transports, allowing
 * the CTAP2 protocol layer to work with either transport seamlessly.
 * Handles transport registration, routing, and coordination.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Transport Types ========== */

/**
 * @brief Transport type enumeration
 */
typedef enum {
    TRANSPORT_TYPE_USB, /**< USB HID transport */
    TRANSPORT_TYPE_BLE, /**< BLE GATT transport */
    TRANSPORT_TYPE_MAX  /**< Number of transport types */
} transport_type_t;

/* ========== Transport Operations ========== */

/**
 * @brief Transport operations structure
 *
 * Each transport must implement these operations to integrate
 * with the transport abstraction layer.
 */
typedef struct {
    /**
     * @brief Send data on this transport
     *
     * @param data Data to send
     * @param len Length of data
     * @return Number of bytes sent, or negative error code
     */
    int (*send)(const uint8_t *data, size_t len);

    /**
     * @brief Receive data from this transport
     *
     * @param data Buffer to receive data
     * @param max_len Maximum length to receive
     * @param cmd Pointer to store command byte (optional, can be NULL)
     * @return Number of bytes received, or negative error code
     */
    int (*receive)(uint8_t *data, size_t max_len, uint8_t *cmd);

    /**
     * @brief Check if transport is connected
     *
     * @return true if connected, false otherwise
     */
    bool (*is_connected)(void);

    /**
     * @brief Get transport name (for logging)
     *
     * @return Transport name string
     */
    const char *(*get_name)(void);
} transport_ops_t;

/* ========== Transport Management ========== */

/**
 * @brief Initialize transport abstraction layer
 *
 * Must be called before any other transport functions.
 *
 * @return 0 on success, negative error code otherwise
 */
int transport_init(void);

/**
 * @brief Register a transport
 *
 * Registers a transport with its operations. Each transport type
 * can only be registered once.
 *
 * @param type Transport type
 * @param ops Transport operations
 * @return 0 on success, negative error code otherwise
 */
int transport_register(transport_type_t type, const transport_ops_t *ops);

/**
 * @brief Unregister a transport
 *
 * @param type Transport type
 * @return 0 on success, negative error code otherwise
 */
int transport_unregister(transport_type_t type);

/**
 * @brief Check if a transport is registered
 *
 * @param type Transport type
 * @return true if registered, false otherwise
 */
bool transport_is_registered(transport_type_t type);

/* ========== Transport Selection ========== */

/**
 * @brief Set active transport
 *
 * Sets which transport should be used for sending responses.
 * Typically called when a request is received on a transport.
 *
 * @param type Transport type
 * @return 0 on success, negative error code otherwise
 */
int transport_set_active(transport_type_t type);

/**
 * @brief Get active transport type
 *
 * @return Active transport type, or TRANSPORT_TYPE_MAX if none active
 */
transport_type_t transport_get_active(void);

/**
 * @brief Check if a transport is currently active
 *
 * @param type Transport type
 * @return true if active, false otherwise
 */
bool transport_is_active(transport_type_t type);

/* ========== Transport Operations ========== */

/**
 * @brief Send data on active transport
 *
 * Sends data on the currently active transport. If no transport is active,
 * returns an error.
 *
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int transport_send(const uint8_t *data, size_t len);

/**
 * @brief Send data on specific transport
 *
 * Sends data on the specified transport, regardless of which is active.
 *
 * @param type Transport type
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int transport_send_on(transport_type_t type, const uint8_t *data, size_t len);

/**
 * @brief Receive data from specific transport
 *
 * Receives data from the specified transport.
 *
 * @param type Transport type
 * @param data Buffer to receive data
 * @param max_len Maximum length to receive
 * @param cmd Pointer to store command byte (optional, can be NULL)
 * @return Number of bytes received, or negative error code
 */
int transport_receive_from(transport_type_t type, uint8_t *data, size_t max_len, uint8_t *cmd);

/**
 * @brief Check if any transport is connected
 *
 * @return true if at least one transport is connected, false otherwise
 */
bool transport_any_connected(void);

/**
 * @brief Check if specific transport is connected
 *
 * @param type Transport type
 * @return true if connected, false otherwise
 */
bool transport_is_connected(transport_type_t type);

/* ========== Operation State Management ========== */

/**
 * @brief Check if an operation is in progress
 *
 * @return true if operation in progress, false otherwise
 */
bool transport_is_busy(void);

/**
 * @brief Set operation in progress flag
 *
 * Should be called when starting to process a CTAP request.
 *
 * @param busy true to set busy, false to clear
 */
void transport_set_busy(bool busy);

/**
 * @brief Lock transport for exclusive operation
 *
 * Prevents other transports from being activated until unlocked.
 * Used to ensure a request/response pair happens on the same transport.
 *
 * @param type Transport type to lock
 * @return 0 on success, negative error code otherwise
 */
int transport_lock(transport_type_t type);

/**
 * @brief Unlock transport
 *
 * Releases the transport lock.
 */
void transport_unlock(void);

/**
 * @brief Check if transport is locked
 *
 * @return true if locked, false otherwise
 */
bool transport_is_locked(void);

/**
 * @brief Get locked transport type
 *
 * @return Locked transport type, or TRANSPORT_TYPE_MAX if not locked
 */
transport_type_t transport_get_locked(void);

/* ========== Error Codes ========== */

#define TRANSPORT_OK 0
#define TRANSPORT_ERROR -1
#define TRANSPORT_ERROR_INVALID_PARAM -2
#define TRANSPORT_ERROR_NOT_INITIALIZED -3
#define TRANSPORT_ERROR_NOT_REGISTERED -4
#define TRANSPORT_ERROR_ALREADY_REGISTERED -5
#define TRANSPORT_ERROR_NO_ACTIVE_TRANSPORT -6
#define TRANSPORT_ERROR_NOT_CONNECTED -7
#define TRANSPORT_ERROR_BUSY -8
#define TRANSPORT_ERROR_LOCKED -9

/* ========== Utility Functions ========== */

/**
 * @brief Get transport type name
 *
 * @param type Transport type
 * @return Transport type name string
 */
const char *transport_type_name(transport_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* TRANSPORT_H */
