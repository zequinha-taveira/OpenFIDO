/**
 * @file oath.h
 * @brief OATH TOTP/HOTP Implementation
 *
 * Based on:
 * - RFC 6238 (TOTP: Time-Based One-Time Password Algorithm)
 * - RFC 4226 (HOTP: HMAC-Based One-Time Password Algorithm)
 * - Yubico YKOATH Protocol Specification
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef OATH_H
#define OATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OATH Algorithm Types */
typedef enum {
    OATH_TYPE_HOTP = 0x01, /* HMAC-based OTP */
    OATH_TYPE_TOTP = 0x02  /* Time-based OTP */
} oath_type_t;

/* OATH Hash Algorithms */
typedef enum {
    OATH_HASH_SHA1 = 0x01,
    OATH_HASH_SHA256 = 0x02,
    OATH_HASH_SHA512 = 0x03
} oath_hash_t;

/* OATH Credential */
typedef struct {
    char name[64];      /* Credential name */
    oath_type_t type;   /* HOTP or TOTP */
    oath_hash_t hash;   /* Hash algorithm */
    uint8_t secret[64]; /* Shared secret */
    size_t secret_len;  /* Secret length */
    uint8_t digits;     /* Number of digits (6 or 8) */
    uint32_t counter;   /* HOTP counter */
    uint32_t period;    /* TOTP period in seconds (default 30) */
} oath_credential_t;

/**
 * @brief Initialize OATH module
 */
void oath_init(void);

/**
 * @brief Add OATH credential
 *
 * @param cred Credential to add
 * @return 0 on success, error code otherwise
 */
int oath_add_credential(const oath_credential_t *cred);

/**
 * @brief Delete OATH credential
 *
 * @param name Credential name
 * @return 0 on success, error code otherwise
 */
int oath_delete_credential(const char *name);

/**
 * @brief List OATH credentials
 *
 * @param names Output buffer for credential names
 * @param max_count Maximum number of credentials to return
 * @param count Output: actual number of credentials
 * @return 0 on success, error code otherwise
 */
int oath_list_credentials(char names[][64], size_t max_count, size_t *count);

/**
 * @brief Generate HOTP code
 *
 * @param secret Shared secret
 * @param secret_len Length of secret
 * @param counter Counter value
 * @param digits Number of digits (6 or 8)
 * @param code Output: generated code
 * @return 0 on success, error code otherwise
 */
int oath_hotp_generate(const uint8_t *secret, size_t secret_len, uint64_t counter, uint8_t digits,
                       uint32_t *code);

/**
 * @brief Generate TOTP code
 *
 * @param secret Shared secret
 * @param secret_len Length of secret
 * @param timestamp Current Unix timestamp
 * @param period Time period in seconds (usually 30)
 * @param digits Number of digits (6 or 8)
 * @param code Output: generated code
 * @return 0 on success, error code otherwise
 */
int oath_totp_generate(const uint8_t *secret, size_t secret_len, uint64_t timestamp,
                       uint32_t period, uint8_t digits, uint32_t *code);

/**
 * @brief Calculate OATH code for credential
 *
 * @param name Credential name
 * @param code Output: generated code
 * @return 0 on success, error code otherwise
 */
int oath_calculate(const char *name, uint32_t *code);

/**
 * @brief Verify OATH code
 *
 * @param name Credential name
 * @param code Code to verify
 * @param window Time window for TOTP (±N periods)
 * @return 0 if valid, error code otherwise
 */
int oath_verify(const char *name, uint32_t code, uint8_t window);

#ifdef __cplusplus
}
#endif

#endif /* OATH_H */
