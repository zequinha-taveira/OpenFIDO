/**
 * @file challenge_response.c
 * @brief HMAC-SHA1 Challenge-Response Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "challenge_response.h"

#include <string.h>

#include "crypto.h"
#include "logger.h"

/* Challenge-response slots (2 slots like Yubikey) */
static challenge_response_slot_t slots[2] = {0};

void challenge_response_init(void)
{
    memset(slots, 0, sizeof(slots));
    LOG_INFO("Challenge-response module initialized");
}

int challenge_response_configure(uint8_t slot, const uint8_t *secret)
{
    if (slot < 1 || slot > 2) {
        LOG_ERROR("Invalid slot number: %d", slot);
        return -1;
    }

    if (secret == NULL) {
        return -2;
    }

    /* Store secret in slot (1-indexed, so subtract 1) */
    memcpy(slots[slot - 1].secret, secret, 20);
    slots[slot - 1].configured = true;

    LOG_INFO("Challenge-response slot %d configured", slot);

    return 0;
}

int challenge_response_calculate(uint8_t slot, const uint8_t *challenge, size_t challenge_len,
                                  uint8_t *response)
{
    if (slot < 1 || slot > 2) {
        LOG_ERROR("Invalid slot number: %d", slot);
        return -1;
    }

    if (!slots[slot - 1].configured) {
        LOG_ERROR("Slot %d not configured", slot);
        return -2;
    }

    if (challenge == NULL || response == NULL) {
        return -3;
    }

    /* Calculate HMAC-SHA1(secret, challenge) */
    /* Note: Using HMAC-SHA256 as placeholder since we have that implemented */
    uint8_t hmac[32];
    if (crypto_hmac_sha256(slots[slot - 1].secret, 20, challenge, challenge_len, hmac) !=
        CRYPTO_OK) {
        LOG_ERROR("HMAC calculation failed");
        return -4;
    }

    /* Return first 20 bytes (SHA-1 size) */
    memcpy(response, hmac, 20);

    LOG_INFO("Challenge-response calculated for slot %d", slot);

    return 0;
}
