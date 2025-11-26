/**
 * @file challenge_response.h
 * @brief HMAC-SHA1 Challenge-Response
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef CHALLENGE_RESPONSE_H
#define CHALLENGE_RESPONSE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Challenge-Response slot */
typedef struct {
    uint8_t secret[20];             /* HMAC-SHA1 secret (20 bytes) */
    bool configured;                /* Is slot configured? */
} challenge_response_slot_t;

/**
 * @brief Initialize challenge-response module
 */
void challenge_response_init(void);

/**
 * @brief Configure challenge-response slot
 *
 * @param slot Slot number (1 or 2)
 * @param secret HMAC secret (20 bytes)
 * @return 0 on success, error code otherwise
 */
int challenge_response_configure(uint8_t slot, const uint8_t *secret);

/**
 * @brief Calculate challenge-response
 *
 * @param slot Slot number (1 or 2)
 * @param challenge Challenge data
 * @param challenge_len Length of challenge
 * @param response Output buffer for response (20 bytes)
 * @return 0 on success, error code otherwise
 */
int challenge_response_calculate(uint8_t slot, const uint8_t *challenge, size_t challenge_len,
                                  uint8_t *response);

#ifdef __cplusplus
}
#endif

#endif /* CHALLENGE_RESPONSE_H */
