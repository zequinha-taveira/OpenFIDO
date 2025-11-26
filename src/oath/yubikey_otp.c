/**
 * @file yubikey_otp.c
 * @brief Yubikey OTP Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "yubikey_otp.h"

#include <string.h>

#include "crypto.h"
#include "logger.h"

/* Modhex alphabet */
static const char modhex_alphabet[] = "cbdefghijklnrtuv";

void yubikey_otp_init(void)
{
    LOG_INFO("Yubikey OTP module initialized");
}

void yubikey_modhex_encode(const uint8_t *data, size_t len, char *modhex)
{
    for (size_t i = 0; i < len; i++) {
        modhex[i * 2] = modhex_alphabet[(data[i] >> 4) & 0x0F];
        modhex[i * 2 + 1] = modhex_alphabet[data[i] & 0x0F];
    }
    modhex[len * 2] = '\0';
}

size_t yubikey_modhex_decode(const char *modhex, uint8_t *data, size_t len)
{
    size_t decoded = 0;

    for (size_t i = 0; i < len && modhex[i] != '\0' && modhex[i + 1] != '\0'; i += 2) {
        uint8_t high = 0, low = 0;

        /* Find high nibble */
        for (int j = 0; j < 16; j++) {
            if (modhex[i] == modhex_alphabet[j]) {
                high = j;
                break;
            }
        }

        /* Find low nibble */
        for (int j = 0; j < 16; j++) {
            if (modhex[i + 1] == modhex_alphabet[j]) {
                low = j;
                break;
            }
        }

        data[decoded++] = (high << 4) | low;
    }

    return decoded;
}

int yubikey_otp_generate(yubikey_otp_config_t *config, char *otp)
{
    /* Build OTP token (16 bytes) */
    uint8_t token[16];
    size_t offset = 0;

    /* Private ID (6 bytes) */
    memcpy(&token[offset], config->private_id, 6);
    offset += 6;

    /* Session counter (2 bytes) */
    token[offset++] = config->counter & 0xFF;
    token[offset++] = (config->counter >> 8) & 0xFF;

    /* Timestamp (3 bytes) */
    token[offset++] = config->timestamp_low;
    token[offset++] = config->timestamp_high & 0xFF;
    token[offset++] = (config->timestamp_high >> 8) & 0xFF;

    /* Session use (1 byte) */
    token[offset++] = config->session_use;

    /* Random (2 bytes) */
    token[offset++] = config->random & 0xFF;
    token[offset++] = (config->random >> 8) & 0xFF;

    /* CRC16 (2 bytes) - simplified, should use proper CRC16 */
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < 14; i++) {
        crc ^= token[i];
    }
    token[offset++] = crc & 0xFF;
    token[offset++] = (crc >> 8) & 0xFF;

    /* Encrypt with AES-128 */
    uint8_t encrypted[16];
    /* Note: This requires AES-128 ECB mode */
    /* For now, simplified - would use crypto_aes_encrypt */
    memcpy(encrypted, token, 16);

    /* Encode public ID in modhex */
    char public_id_modhex[13];
    yubikey_modhex_encode(config->public_id, 6, public_id_modhex);

    /* Encode encrypted token in modhex */
    char token_modhex[33];
    yubikey_modhex_encode(encrypted, 16, token_modhex);

    /* Combine: public_id + encrypted_token */
    strcpy(otp, public_id_modhex);
    strcat(otp, token_modhex);

    /* Increment counters */
    config->session_use++;
    if (config->session_use == 0) {
        config->counter++;
    }

    LOG_INFO("Yubikey OTP generated: %s", otp);

    return 0;
}

int yubikey_otp_verify(const char *otp, const yubikey_otp_config_t *config)
{
    /* TODO: Implement OTP verification */
    /* Would decrypt and verify CRC, counters, etc. */
    return -1; /* Not implemented */
}
