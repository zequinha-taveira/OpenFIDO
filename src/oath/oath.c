/**
 * @file oath.c
 * @brief OATH TOTP/HOTP Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "oath.h"

#include <string.h>

#include "crypto.h"
#include "logger.h"

/* Maximum number of OATH credentials */
#define MAX_OATH_CREDENTIALS 32

/* OATH credential storage */
static struct {
    oath_credential_t credentials[MAX_OATH_CREDENTIALS];
    size_t count;
    bool initialized;
} oath_storage = {0};

void oath_init(void)
{
    memset(&oath_storage, 0, sizeof(oath_storage));
    oath_storage.initialized = true;
    LOG_INFO("OATH module initialized");
}

int oath_add_credential(const oath_credential_t *cred)
{
    if (!oath_storage.initialized) {
        return -1;
    }

    if (oath_storage.count >= MAX_OATH_CREDENTIALS) {
        LOG_ERROR("OATH credential storage full");
        return -2;
    }

    /* Check for duplicate name */
    for (size_t i = 0; i < oath_storage.count; i++) {
        if (strcmp(oath_storage.credentials[i].name, cred->name) == 0) {
            LOG_ERROR("OATH credential already exists: %s", cred->name);
            return -3;
        }
    }

    /* Add credential */
    memcpy(&oath_storage.credentials[oath_storage.count], cred, sizeof(oath_credential_t));
    oath_storage.count++;

    LOG_INFO("OATH credential added: %s (type=%d, digits=%d)", cred->name, cred->type,
             cred->digits);

    return 0;
}

int oath_delete_credential(const char *name)
{
    if (!oath_storage.initialized) {
        return -1;
    }

    /* Find credential */
    for (size_t i = 0; i < oath_storage.count; i++) {
        if (strcmp(oath_storage.credentials[i].name, name) == 0) {
            /* Remove by shifting remaining credentials */
            memmove(&oath_storage.credentials[i], &oath_storage.credentials[i + 1],
                    (oath_storage.count - i - 1) * sizeof(oath_credential_t));
            oath_storage.count--;

            LOG_INFO("OATH credential deleted: %s", name);
            return 0;
        }
    }

    LOG_ERROR("OATH credential not found: %s", name);
    return -2;
}

int oath_list_credentials(char names[][64], size_t max_count, size_t *count)
{
    if (!oath_storage.initialized) {
        return -1;
    }

    size_t num_to_copy = (oath_storage.count < max_count) ? oath_storage.count : max_count;

    for (size_t i = 0; i < num_to_copy; i++) {
        strncpy(names[i], oath_storage.credentials[i].name, 63);
        names[i][63] = '\0';
    }

    *count = num_to_copy;
    return 0;
}

int oath_hotp_generate(const uint8_t *secret, size_t secret_len, uint64_t counter, uint8_t digits,
                       uint32_t *code)
{
    /* HOTP = Truncate(HMAC-SHA1(secret, counter)) */

    /* Convert counter to big-endian bytes */
    uint8_t counter_bytes[8];
    for (int i = 7; i >= 0; i--) {
        counter_bytes[i] = counter & 0xFF;
        counter >>= 8;
    }

    /* Calculate HMAC-SHA1 */
    uint8_t hmac[32];
    if (crypto_hmac_sha256(secret, secret_len, counter_bytes, 8, hmac) != CRYPTO_OK) {
        return -1;
    }

    /* Dynamic truncation (RFC 4226 Section 5.3) */
    uint8_t offset = hmac[31] & 0x0F;
    uint32_t binary = ((hmac[offset] & 0x7F) << 24) | ((hmac[offset + 1] & 0xFF) << 16) |
                      ((hmac[offset + 2] & 0xFF) << 8) | (hmac[offset + 3] & 0xFF);

    /* Generate code with specified number of digits */
    uint32_t modulo = 1;
    for (uint8_t i = 0; i < digits; i++) {
        modulo *= 10;
    }

    *code = binary % modulo;

    return 0;
}

int oath_totp_generate(const uint8_t *secret, size_t secret_len, uint64_t timestamp,
                       uint32_t period, uint8_t digits, uint32_t *code)
{
    /* TOTP = HOTP(secret, T) where T = floor(timestamp / period) */
    uint64_t T = timestamp / period;
    return oath_hotp_generate(secret, secret_len, T, digits, code);
}

int oath_calculate(const char *name, uint32_t *code)
{
    if (!oath_storage.initialized) {
        return -1;
    }

    /* Find credential */
    oath_credential_t *cred = NULL;
    for (size_t i = 0; i < oath_storage.count; i++) {
        if (strcmp(oath_storage.credentials[i].name, name) == 0) {
            cred = &oath_storage.credentials[i];
            break;
        }
    }

    if (cred == NULL) {
        LOG_ERROR("OATH credential not found: %s", name);
        return -2;
    }

    /* Generate code based on type */
    int result;
    if (cred->type == OATH_TYPE_HOTP) {
        result =
            oath_hotp_generate(cred->secret, cred->secret_len, cred->counter, cred->digits, code);
        if (result == 0) {
            /* Increment counter for next use */
            cred->counter++;
        }
    } else if (cred->type == OATH_TYPE_TOTP) {
        /* Get current timestamp (would need RTC or time sync) */
        uint64_t timestamp = 0; /* TODO: Get actual timestamp */
        result = oath_totp_generate(cred->secret, cred->secret_len, timestamp, cred->period,
                                    cred->digits, code);
    } else {
        return -3;
    }

    if (result == 0) {
        LOG_INFO("OATH code generated for %s: %0*u", name, cred->digits, *code);
    }

    return result;
}

int oath_verify(const char *name, uint32_t code, uint8_t window)
{
    /* TODO: Implement OATH code verification with time window */
    return -1; /* Not implemented */
}
