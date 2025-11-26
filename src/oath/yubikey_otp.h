/**
 * @file yubikey_otp.h
 * @brief Yubikey OTP Implementation
 *
 * Implements Yubikey OTP format (AES-128 encrypted modhex)
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef YUBIKEY_OTP_H
#define YUBIKEY_OTP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Yubikey OTP Configuration */
typedef struct {
    uint8_t public_id[6];    /* Public ID (6 bytes) */
    uint8_t private_id[6];   /* Private ID (6 bytes) */
    uint8_t aes_key[16];     /* AES-128 key */
    uint16_t counter;        /* Session counter */
    uint8_t timestamp_low;   /* Timestamp low byte */
    uint16_t timestamp_high; /* Timestamp high word */
    uint8_t session_use;     /* Session use counter */
    uint16_t random;         /* Random number */
} yubikey_otp_config_t;

/**
 * @brief Initialize Yubikey OTP module
 */
void yubikey_otp_init(void);

/**
 * @brief Generate Yubikey OTP
 *
 * @param config OTP configuration
 * @param otp Output buffer (44 characters + null terminator)
 * @return 0 on success, error code otherwise
 */
int yubikey_otp_generate(yubikey_otp_config_t *config, char *otp);

/**
 * @brief Verify Yubikey OTP
 *
 * @param otp OTP string to verify
 * @param config Configuration to verify against
 * @return 0 if valid, error code otherwise
 */
int yubikey_otp_verify(const char *otp, const yubikey_otp_config_t *config);

/**
 * @brief Convert bytes to modhex
 *
 * @param data Input data
 * @param len Length of data
 * @param modhex Output modhex string
 */
void yubikey_modhex_encode(const uint8_t *data, size_t len, char *modhex);

/**
 * @brief Convert modhex to bytes
 *
 * @param modhex Input modhex string
 * @param data Output data buffer
 * @param len Length of modhex string
 * @return Number of bytes decoded
 */
size_t yubikey_modhex_decode(const char *modhex, uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* YUBIKEY_OTP_H */
