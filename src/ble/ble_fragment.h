/**
 * @file ble_fragment.h
 * @brief BLE Message Fragmentation and Reassembly
 *
 * Handles fragmentation and reassembly of CTAP messages for BLE transport.
 * BLE has limited MTU size, so large CTAP messages must be split into
 * multiple fragments for transmission.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef BLE_FRAGMENT_H
#define BLE_FRAGMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Fragment Types ========== */

/**
 * @brief Fragment type: Initial fragment
 *
 * Initial fragment contains the total message length in the first two bytes
 * after the command byte.
 */
#define BLE_FRAGMENT_TYPE_INIT 0x80

/**
 * @brief Fragment type: Continuation fragment
 *
 * Continuation fragments contain only data, no length information.
 */
#define BLE_FRAGMENT_TYPE_CONT 0x00

/* ========== Fragment Configuration ========== */

/**
 * @brief Maximum CTAP message size (7609 bytes per CTAP spec)
 */
#define BLE_FRAGMENT_MAX_MESSAGE_SIZE 7609

/**
 * @brief Default BLE MTU size
 */
#define BLE_FRAGMENT_DEFAULT_MTU 23

/**
 * @brief Maximum number of fragments for a message
 */
#define BLE_FRAGMENT_MAX_FRAGMENTS 128

/* ========== Fragment Buffer Structure ========== */

/**
 * @brief Fragment reassembly buffer
 *
 * Maintains state for reassembling fragmented messages received over BLE.
 */
typedef struct {
    uint8_t *buffer;     /**< Message buffer */
    size_t total_len;    /**< Total expected message length */
    size_t received_len; /**< Bytes received so far */
    uint8_t seq;         /**< Expected sequence number */
    bool in_progress;    /**< Reassembly in progress */
} ble_fragment_buffer_t;

/* ========== Fragment Buffer Management ========== */

/**
 * @brief Initialize fragment buffer
 *
 * Prepares a fragment buffer for message reassembly.
 *
 * @param frag Pointer to fragment buffer structure
 */
void ble_fragment_init(ble_fragment_buffer_t *frag);

/**
 * @brief Add fragment to buffer
 *
 * Adds a received fragment to the reassembly buffer. Handles both
 * initial and continuation fragments.
 *
 * @param frag Pointer to fragment buffer structure
 * @param data Fragment data
 * @param len Fragment data length
 * @return 0 on success, negative error code otherwise
 */
int ble_fragment_add(ble_fragment_buffer_t *frag, const uint8_t *data, size_t len);

/**
 * @brief Check if message is complete
 *
 * Returns true when all fragments have been received and the message
 * is ready for processing.
 *
 * @param frag Pointer to fragment buffer structure
 * @return true if message is complete, false otherwise
 */
bool ble_fragment_is_complete(const ble_fragment_buffer_t *frag);

/**
 * @brief Get complete message
 *
 * Retrieves the reassembled message after all fragments have been received.
 *
 * @param frag Pointer to fragment buffer structure
 * @param data Output pointer to message data
 * @param len Output pointer to message length
 * @return 0 on success, negative error code otherwise
 */
int ble_fragment_get_message(const ble_fragment_buffer_t *frag, uint8_t **data, size_t *len);

/**
 * @brief Reset fragment buffer
 *
 * Clears the fragment buffer and prepares it for a new message.
 *
 * @param frag Pointer to fragment buffer structure
 */
void ble_fragment_reset(ble_fragment_buffer_t *frag);

/* ========== Fragment Creation ========== */

/**
 * @brief Fragment outgoing message
 *
 * Splits a CTAP message into fragments suitable for BLE transmission.
 * The first fragment is an INIT fragment containing the total length,
 * followed by CONT fragments.
 *
 * @param data Message data to fragment
 * @param len Message length
 * @param fragments Output array of fragment pointers
 * @param fragment_sizes Output array of fragment sizes
 * @param num_fragments Output pointer to number of fragments created
 * @param mtu Maximum transmission unit size
 * @return 0 on success, negative error code otherwise
 */
int ble_fragment_create(const uint8_t *data, size_t len, uint8_t *fragments[],
                        size_t fragment_sizes[], size_t *num_fragments, size_t mtu);

/* ========== Error Codes ========== */

#define BLE_FRAGMENT_OK 0
#define BLE_FRAGMENT_ERROR -1
#define BLE_FRAGMENT_ERROR_INVALID_PARAM -2
#define BLE_FRAGMENT_ERROR_NO_MEM -3
#define BLE_FRAGMENT_ERROR_INVALID_SEQ -4
#define BLE_FRAGMENT_ERROR_TOO_LARGE -5
#define BLE_FRAGMENT_ERROR_INCOMPLETE -6

#ifdef __cplusplus
}
#endif

#endif /* BLE_FRAGMENT_H */
