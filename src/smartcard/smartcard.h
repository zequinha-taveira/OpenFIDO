/**
 * @file smartcard.h
 * @brief Smart Card Protocol Abstraction
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef SMARTCARD_H
#define SMARTCARD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Smart Card Protocol Interface
 */
typedef struct {
    /**
     * @brief Initialize the protocol
     * @return 0 on success, error code otherwise
     */
    int (*init)(void);

    /**
     * @brief Handle APDU command
     * @param cmd APDU command buffer
     * @param resp Response buffer
     * @return 0 on success, error code otherwise
     */
    int (*handle_apdu)(const void *cmd, void *resp);

    /**
     * @brief Reset protocol state
     * @return 0 on success, error code otherwise
     */
    int (*reset)(void);
} smartcard_protocol_t;

/**
 * @brief Register a smart card protocol
 *
 * @param aid Application Identifier
 * @param aid_len Length of AID
 * @param protocol Protocol implementation
 * @return 0 on success, error code otherwise
 */
int smartcard_register_protocol(const uint8_t *aid, size_t aid_len,
                                const smartcard_protocol_t *protocol);

/**
 * @brief Dispatch APDU to registered protocols
 *
 * @param apdu APDU command
 * @param len Length of APDU
 * @param resp Response buffer
 * @param resp_len Output: response length
 * @return 0 on success, error code otherwise
 */
int smartcard_dispatch_apdu(const uint8_t *apdu, size_t len, uint8_t *resp, size_t *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* SMARTCARD_H */
