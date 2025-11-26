/**
 * @file u2f.c
 * @brief U2F (CTAP1) Protocol Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "u2f.h"

#include <string.h>

#include "crypto.h"
#include "logger.h"
#include "storage.h"

#define U2F_VERSION_STRING "U2F_V2"

uint16_t u2f_process_apdu(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                          size_t *response_len)
{
    if (request_len < 4) {
        return U2F_SW_WRONG_DATA;
    }

    uint8_t cla = request_data[0];
    uint8_t ins = request_data[1];
    uint8_t p1 = request_data[2];
    uint8_t p2 = request_data[3];

    /* Parse Lc and Data if present */
    size_t data_offset = 4;
    size_t data_len = 0;

    if (request_len > 4) {
        /* Extended length encoding check */
        if (request_data[4] == 0 && request_len > 6) {
            data_len = (request_data[5] << 8) | request_data[6];
            data_offset = 7;
        } else {
            data_len = request_data[4];
            data_offset = 5;
        }
    }

    const uint8_t *data = (request_len > data_offset) ? &request_data[data_offset] : NULL;

    LOG_DEBUG("U2F APDU: CLA=%02X INS=%02X P1=%02X P2=%02X Lc=%zu", cla, ins, p1, p2, data_len);

    switch (ins) {
        case U2F_VERSION:
            if (data_len != 0) {
                return U2F_SW_WRONG_DATA;
            }
            memcpy(response_data, U2F_VERSION_STRING, 6);
            *response_len = 6;
            return U2F_SW_NO_ERROR;

        case U2F_REGISTER: {
            /* U2F Register: challenge (32) + application (32) = 64 bytes */
            if (data_len != 64) {
                LOG_ERROR("U2F Register: invalid data length %zu", data_len);
                return U2F_SW_WRONG_DATA;
            }

            const uint8_t *challenge = data;
            const uint8_t *application = data + 32;

            /* Request user presence */
            LOG_INFO("U2F Register: Waiting for user presence...");
            hal_led_set_state(HAL_LED_BLINK_FAST);
            if (!hal_button_wait_press(30000)) {
                hal_led_set_state(HAL_LED_OFF);
                return U2F_SW_CONDITIONS_NOT_SATISFIED;
            }
            hal_led_set_state(HAL_LED_ON);

            /* Generate key pair */
            uint8_t private_key[32];
            uint8_t public_key[64];

            if (crypto_ecdsa_generate_keypair(private_key, public_key) != CRYPTO_OK) {
                return U2F_SW_COMMAND_NOT_ALLOWED;
            }

            /* Create credential */
            storage_credential_t credential = {0};
            crypto_random_generate(credential.id, STORAGE_CREDENTIAL_ID_LENGTH);
            memcpy(credential.rp_id_hash, application, 32);
            memcpy(credential.private_key, private_key, 32);
            credential.algorithm = COSE_ALG_ES256;
            credential.resident = false;
            storage_get_and_increment_counter(&credential.sign_count);

            /* Store credential */
            if (storage_store_credential(&credential) != STORAGE_OK) {
                return U2F_SW_COMMAND_NOT_ALLOWED;
            }

            /* Build registration response */
            size_t offset = 0;

            /* Reserved byte */
            response_data[offset++] = 0x05;

            /* Public key (65 bytes: 0x04 + X + Y) */
            response_data[offset++] = 0x04;
            memcpy(&response_data[offset], public_key, 64);
            offset += 64;

            /* Key handle length */
            response_data[offset++] = STORAGE_CREDENTIAL_ID_LENGTH;

            /* Key handle */
            memcpy(&response_data[offset], credential.id, STORAGE_CREDENTIAL_ID_LENGTH);
            offset += STORAGE_CREDENTIAL_ID_LENGTH;

            /* Attestation certificate (self-signed, minimal) */
            /* For simplicity, we'll use a minimal self-signed cert */
            /* In production, this should be a proper X.509 certificate */
            uint8_t cert[] = {
                0x30, 0x59, /* SEQUENCE, length 89 */
                0x30, 0x13, /* SEQUENCE, length 19 (tbsCertificate minimal) */
                0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,       /* OID ecPublicKey */
                0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, /* OID prime256v1 */
                0x03, 0x42, 0x00, /* BIT STRING, length 66 */
                0x04              /* Uncompressed point */
            };
            memcpy(&response_data[offset], cert, sizeof(cert));
            offset += sizeof(cert);

            /* Add public key to cert */
            memcpy(&response_data[offset], public_key, 64);
            offset += 64;

            /* Signature over registration data */
            uint8_t sig_data[256];
            size_t sig_data_len = 0;

            sig_data[sig_data_len++] = 0x00; /* Reserved byte */
            memcpy(&sig_data[sig_data_len], application, 32);
            sig_data_len += 32;
            memcpy(&sig_data[sig_data_len], challenge, 32);
            sig_data_len += 32;
            memcpy(&sig_data[sig_data_len], credential.id, STORAGE_CREDENTIAL_ID_LENGTH);
            sig_data_len += STORAGE_CREDENTIAL_ID_LENGTH;
            sig_data[sig_data_len++] = 0x04;
            memcpy(&sig_data[sig_data_len], public_key, 64);
            sig_data_len += 64;

            /* Hash and sign */
            uint8_t hash[32];
            crypto_sha256(sig_data, sig_data_len, hash);

            uint8_t signature[64];
            uint8_t att_key[32];
            storage_get_attestation_key(att_key);

            if (crypto_ecdsa_sign(att_key, hash, signature) != CRYPTO_OK) {
                return U2F_SW_COMMAND_NOT_ALLOWED;
            }

            /* Encode signature in DER format */
            response_data[offset++] = 0x30; /* SEQUENCE */
            size_t sig_len_pos = offset++;
            response_data[offset++] = 0x02; /* INTEGER r */
            response_data[offset++] = 32;
            memcpy(&response_data[offset], signature, 32);
            offset += 32;
            response_data[offset++] = 0x02; /* INTEGER s */
            response_data[offset++] = 32;
            memcpy(&response_data[offset], &signature[32], 32);
            offset += 32;
            response_data[sig_len_pos] = offset - sig_len_pos - 1;

            *response_len = offset;
            memset(private_key, 0, sizeof(private_key));
            memset(att_key, 0, sizeof(att_key));

            hal_led_set_state(HAL_LED_OFF);
            return U2F_SW_NO_ERROR;
        }

        case U2F_AUTHENTICATE: {
            /* U2F Authenticate: challenge (32) + application (32) + key_handle_len (1) + key_handle
             */
            if (data_len < 65) {
                return U2F_SW_WRONG_DATA;
            }

            const uint8_t *challenge = data;
            const uint8_t *application = data + 32;
            uint8_t key_handle_len = data[64];
            const uint8_t *key_handle = data + 65;

            if (data_len < 65 + key_handle_len) {
                return U2F_SW_WRONG_DATA;
            }

            /* Find credential */
            storage_credential_t credential;
            if (storage_find_credential(key_handle, &credential) != STORAGE_OK) {
                return U2F_SW_WRONG_DATA;
            }

            /* Verify application parameter matches */
            if (memcmp(credential.rp_id_hash, application, 32) != 0) {
                return U2F_SW_WRONG_DATA;
            }

            /* Check-only mode */
            if (p1 == U2F_AUTH_CHECK_ONLY) {
                return U2F_SW_CONDITIONS_NOT_SATISFIED;
            }

            /* Request user presence */
            LOG_INFO("U2F Authenticate: Waiting for user presence...");
            hal_led_set_state(HAL_LED_BLINK_FAST);
            if (!hal_button_wait_press(30000)) {
                hal_led_set_state(HAL_LED_OFF);
                return U2F_SW_CONDITIONS_NOT_SATISFIED;
            }
            hal_led_set_state(HAL_LED_ON);

            /* Increment counter */
            uint32_t counter;
            storage_get_and_increment_counter(&counter);
            storage_update_sign_count(key_handle, counter);

            /* Build authentication data */
            uint8_t auth_data[256];
            size_t auth_data_len = 0;

            auth_data[auth_data_len++] = 0x01; /* User presence */
            auth_data[auth_data_len++] = (counter >> 24) & 0xFF;
            auth_data[auth_data_len++] = (counter >> 16) & 0xFF;
            auth_data[auth_data_len++] = (counter >> 8) & 0xFF;
            auth_data[auth_data_len++] = counter & 0xFF;

            /* Build signature data */
            uint8_t sig_data[256];
            size_t sig_data_len = 0;

            memcpy(&sig_data[sig_data_len], application, 32);
            sig_data_len += 32;
            memcpy(&sig_data[sig_data_len], auth_data, auth_data_len);
            sig_data_len += auth_data_len;
            memcpy(&sig_data[sig_data_len], challenge, 32);
            sig_data_len += 32;

            /* Hash and sign */
            uint8_t hash[32];
            crypto_sha256(sig_data, sig_data_len, hash);

            uint8_t signature[64];
            if (crypto_ecdsa_sign(credential.private_key, hash, signature) != CRYPTO_OK) {
                return U2F_SW_COMMAND_NOT_ALLOWED;
            }

            /* Build response */
            size_t offset = 0;
            memcpy(&response_data[offset], auth_data, auth_data_len);
            offset += auth_data_len;

            /* Encode signature in DER format */
            response_data[offset++] = 0x30; /* SEQUENCE */
            size_t sig_len_pos = offset++;
            response_data[offset++] = 0x02; /* INTEGER r */
            response_data[offset++] = 32;
            memcpy(&response_data[offset], signature, 32);
            offset += 32;
            response_data[offset++] = 0x02; /* INTEGER s */
            response_data[offset++] = 32;
            memcpy(&response_data[offset], &signature[32], 32);
            offset += 32;
            response_data[sig_len_pos] = offset - sig_len_pos - 1;

            *response_len = offset;
            hal_led_set_state(HAL_LED_OFF);
            return U2F_SW_NO_ERROR;
        }

        default:
            return U2F_SW_INS_NOT_SUPPORTED;
    }
}
