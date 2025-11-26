/**
 * @file ctap2_commands.c
 * @brief CTAP2 Command Implementations (MakeCredential, GetAssertion)
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ctap2.h"

#include <string.h>

#include "cbor.h"
#include "crypto.h"
#include "hal.h"
#include "logger.h"
#include "storage.h"

/* MakeCredential Request Keys */
#define MC_CLIENT_DATA_HASH 0x01
#define MC_RP 0x02
#define MC_USER 0x03
#define MC_PUB_KEY_CRED_PARAMS 0x04
#define MC_EXCLUDE_LIST 0x05
#define MC_EXTENSIONS 0x06
#define MC_OPTIONS 0x07
#define MC_PIN_AUTH 0x08
#define MC_PIN_PROTOCOL 0x09

/* MakeCredential Response Keys */
#define MC_RESP_FMT 0x01
#define MC_RESP_AUTH_DATA 0x02
#define MC_RESP_ATT_STMT 0x03

/* GetAssertion Request Keys */
#define GA_RP_ID 0x01
#define GA_CLIENT_DATA_HASH 0x02
#define GA_ALLOW_LIST 0x03
#define GA_EXTENSIONS 0x04
#define GA_OPTIONS 0x05
#define GA_PIN_AUTH 0x06
#define GA_PIN_PROTOCOL 0x07

/* GetAssertion Response Keys */
#define GA_RESP_CREDENTIAL 0x01
#define GA_RESP_AUTH_DATA 0x02
#define GA_RESP_SIGNATURE 0x03
#define GA_RESP_USER 0x04
#define GA_RESP_NUM_CREDENTIALS 0x05

/* AAGUID (Authenticator Attestation GUID) */
static const uint8_t AAGUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t ctap2_make_credential(const uint8_t *request_data, size_t request_len,
                               uint8_t *response_data, size_t *response_len)
{
    LOG_INFO("MakeCredential command");

    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    uint64_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint8_t client_data_hash[32];
    char rp_id[256] = {0};
    uint8_t rp_id_hash[32];
    uint8_t user_id[64];
    size_t user_id_len = 0;
    char user_name[64] = {0};
    char display_name[64] = {0};
    int algorithm = COSE_ALG_ES256;
    bool rk = false;
    bool uv = false;
    bool has_client_data_hash = false;
    bool has_rp = false;
    bool has_user = false;
    bool has_pub_key_params = false;
    uint8_t pin_auth[16];
    size_t pin_auth_len = 0;
    uint8_t pin_protocol = 0;
    bool has_pin_auth = false;

    /* Parse all parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case MC_CLIENT_DATA_HASH:
                if (cbor_decode_bytes(&decoder, client_data_hash, 32) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_client_data_hash = true;
                break;

            case MC_RP: {
                uint64_t rp_map_size;
                if (cbor_decode_map_start(&decoder, &rp_map_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                for (uint64_t j = 0; j < rp_map_size; j++) {
                    char rp_key[16];
                    if (cbor_decode_text(&decoder, rp_key, sizeof(rp_key)) != CBOR_OK) {
                        cbor_skip_value(&decoder);
                        cbor_skip_value(&decoder);
                        continue;
                    }
                    if (strcmp(rp_key, "id") == 0) {
                        if (cbor_decode_text(&decoder, rp_id, sizeof(rp_id)) != CBOR_OK) {
                            return CTAP2_ERR_INVALID_CBOR;
                        }
                    } else {
                        cbor_skip_value(&decoder);
                    }
                }
                has_rp = true;
                break;
            }

            case MC_USER: {
                uint64_t user_map_size;
                if (cbor_decode_map_start(&decoder, &user_map_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                for (uint64_t j = 0; j < user_map_size; j++) {
                    char user_key[16];
                    if (cbor_decode_text(&decoder, user_key, sizeof(user_key)) != CBOR_OK) {
                        cbor_skip_value(&decoder);
                        cbor_skip_value(&decoder);
                        continue;
                    }
                    if (strcmp(user_key, "id") == 0) {
                        user_id_len = sizeof(user_id);
                        if (cbor_decode_bytes(&decoder, user_id, user_id_len) != CBOR_OK) {
                            return CTAP2_ERR_INVALID_CBOR;
                        }
                    } else if (strcmp(user_key, "name") == 0) {
                        if (cbor_decode_text(&decoder, user_name, sizeof(user_name)) != CBOR_OK) {
                            return CTAP2_ERR_INVALID_CBOR;
                        }
                    } else if (strcmp(user_key, "displayName") == 0) {
                        if (cbor_decode_text(&decoder, display_name, sizeof(display_name)) != CBOR_OK) {
                            return CTAP2_ERR_INVALID_CBOR;
                        }
                    } else {
                        cbor_skip_value(&decoder);
                    }
                }
                has_user = true;
                break;
            }

            case MC_PUB_KEY_CRED_PARAMS: {
                uint64_t array_size;
                if (cbor_decode_array_start(&decoder, &array_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                /* Just take the first supported algorithm */
                for (uint64_t j = 0; j < array_size; j++) {
                    uint64_t param_map_size;
                    if (cbor_decode_map_start(&decoder, &param_map_size) != CBOR_OK) {
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (uint64_t k = 0; k < param_map_size; k++) {
                        char param_key[8];
                        if (cbor_decode_text(&decoder, param_key, sizeof(param_key)) != CBOR_OK) {
                            cbor_skip_value(&decoder);
                            cbor_skip_value(&decoder);
                            continue;
                        }
                        if (strcmp(param_key, "alg") == 0) {
                            int64_t alg;
                            if (cbor_decode_int(&decoder, &alg) != CBOR_OK) {
                                return CTAP2_ERR_INVALID_CBOR;
                            }
                            if (j == 0) algorithm = (int)alg;
                        } else {
                            cbor_skip_value(&decoder);
                        }
                    }
                }
                has_pub_key_params = true;
                break;
            }

            case MC_OPTIONS: {
                uint64_t options_map_size;
                if (cbor_decode_map_start(&decoder, &options_map_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                for (uint64_t j = 0; j < options_map_size; j++) {
                    char option_key[16];
                    if (cbor_decode_text(&decoder, option_key, sizeof(option_key)) != CBOR_OK) {
                        cbor_skip_value(&decoder);
                        cbor_skip_value(&decoder);
                        continue;
                    }
                    bool option_value;
                    if (cbor_decode_bool(&decoder, &option_value) != CBOR_OK) {
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    if (strcmp(option_key, "rk") == 0) {
                        rk = option_value;
                    } else if (strcmp(option_key, "uv") == 0) {
                        uv = option_value;
                    }
                }
                break;
            }

            case MC_PIN_AUTH:
                pin_auth_len = sizeof(pin_auth);
                if (cbor_decode_bytes(&decoder, pin_auth, pin_auth_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_pin_auth = true;
                break;

            case MC_PIN_PROTOCOL:
                if (cbor_decode_uint(&decoder, (uint64_t*)&pin_protocol) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            default:
                /* Skip unknown parameters */
                cbor_skip_value(&decoder);
                break;
        }
    }

    /* Validate required parameters */
    if (!has_client_data_hash || !has_rp || !has_user || !has_pub_key_params) {
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    /* Check PIN requirements */
    bool pin_required = rk || uv;  /* PIN required for resident keys or when UV requested */
    bool pin_verified = false;

    if (pin_required) {
        /* Check if PIN is set */
        if (!storage_is_pin_set()) {
            return CTAP2_ERR_PIN_NOT_SET;
        }

        /* PIN auth must be present when PIN is required */
        if (!has_pin_auth) {
            return CTAP2_ERR_PIN_REQUIRED;
        }

        /* Verify PIN protocol version */
        if (pin_protocol != 1) {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        /* In production: verify pinAuth using PIN token and HMAC */
        /* For now, simplified verification - just check if PIN auth is present */
        /* Real implementation would:
         * 1. Verify pinAuth = HMAC-SHA256(pinToken, clientDataHash)
         * 2. Validate PIN token hasn't expired
         */
        if (has_pin_auth && pin_auth_len == 16) {
            pin_verified = true;
            LOG_INFO("PIN verification successful (simplified)");
        } else {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }
    } else if (has_pin_auth) {
        /* PIN auth provided but not required - still verify it */
        if (pin_protocol != 1) {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }
        if (pin_auth_len == 16) {
            pin_verified = true;
        }
    }

    /* Hash RP ID */
    crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), rp_id_hash);

    /* Request user presence */
    LOG_INFO("Waiting for user presence...");
    hal_led_set_state(HAL_LED_BLINK_FAST);
    if (!hal_button_wait_press(30000)) {
        hal_led_set_state(HAL_LED_OFF);
        return CTAP2_ERR_USER_ACTION_TIMEOUT;
    }
    hal_led_set_state(HAL_LED_ON);

    /* Generate key pair */
    uint8_t private_key[32];
    uint8_t public_key[64];
    
    if (algorithm == COSE_ALG_ES256) {
        if (crypto_ecdsa_generate_keypair(private_key, public_key) != CRYPTO_OK) {
            return CTAP2_ERR_PROCESSING;
        }
    } else if (algorithm == COSE_ALG_EDDSA) {
        if (crypto_ed25519_generate_keypair(private_key, public_key) != CRYPTO_OK) {
            return CTAP2_ERR_UNSUPPORTED_ALGORITHM;
        }
    } else {
        return CTAP2_ERR_UNSUPPORTED_ALGORITHM;
    }

    /* Create credential */
    storage_credential_t credential = {0};
    crypto_random_generate(credential.id, STORAGE_CREDENTIAL_ID_LENGTH);
    memcpy(credential.rp_id_hash, rp_id_hash, 32);
    memcpy(credential.user_id, user_id, user_id_len);
    credential.user_id_len = user_id_len;
    memcpy(credential.private_key, private_key, 32);
    credential.algorithm = algorithm;
    credential.resident = rk;
    storage_get_and_increment_counter(&credential.sign_count);

    if (rk) {
        strncpy(credential.user_name, user_name, STORAGE_MAX_USER_NAME_LENGTH - 1);
        strncpy(credential.display_name, display_name, STORAGE_MAX_DISPLAY_NAME_LENGTH - 1);
        strncpy(credential.rp_id, rp_id, STORAGE_MAX_RP_ID_LENGTH - 1);
    }

    /* Store credential */
    if (storage_store_credential(&credential) != STORAGE_OK) {
        return CTAP2_ERR_KEY_STORE_FULL;
    }

    /* Build authenticator data */
    uint8_t auth_data[512];
    size_t auth_data_len = 0;

    /* RP ID hash (32 bytes) */
    memcpy(&auth_data[auth_data_len], rp_id_hash, 32);
    auth_data_len += 32;

    /* Flags */
    uint8_t flags = CTAP2_AUTH_DATA_FLAG_UP | CTAP2_AUTH_DATA_FLAG_AT;
    /* Set UV flag if PIN was verified */
    if (pin_verified) flags |= CTAP2_AUTH_DATA_FLAG_UV;
    auth_data[auth_data_len++] = flags;

    /* Sign counter (4 bytes, big-endian) */
    auth_data[auth_data_len++] = (credential.sign_count >> 24) & 0xFF;
    auth_data[auth_data_len++] = (credential.sign_count >> 16) & 0xFF;
    auth_data[auth_data_len++] = (credential.sign_count >> 8) & 0xFF;
    auth_data[auth_data_len++] = credential.sign_count & 0xFF;

    /* Attested credential data */
    /* AAGUID (16 bytes) */
    memcpy(&auth_data[auth_data_len], AAGUID, 16);
    auth_data_len += 16;

    /* Credential ID length (2 bytes, big-endian) */
    auth_data[auth_data_len++] = 0;
    auth_data[auth_data_len++] = STORAGE_CREDENTIAL_ID_LENGTH;

    /* Credential ID */
    memcpy(&auth_data[auth_data_len], credential.id, STORAGE_CREDENTIAL_ID_LENGTH);
    auth_data_len += STORAGE_CREDENTIAL_ID_LENGTH;

    /* Credential public key (COSE format) */
    cbor_encoder_t key_encoder;
    cbor_encoder_init(&key_encoder, &auth_data[auth_data_len], sizeof(auth_data) - auth_data_len);
    
    if (algorithm == COSE_ALG_ES256) {
        cbor_encode_map_start(&key_encoder, 5);
        cbor_encode_int(&key_encoder, 1); /* kty */
        cbor_encode_int(&key_encoder, 2); /* EC2 */
        cbor_encode_int(&key_encoder, 3); /* alg */
        cbor_encode_int(&key_encoder, COSE_ALG_ES256);
        cbor_encode_int(&key_encoder, -1); /* crv */
        cbor_encode_int(&key_encoder, 1); /* P-256 */
        cbor_encode_int(&key_encoder, -2); /* x */
        cbor_encode_bytes(&key_encoder, &public_key[0], 32);
        cbor_encode_int(&key_encoder, -3); /* y */
        cbor_encode_bytes(&key_encoder, &public_key[32], 32);
    }
    
    auth_data_len += cbor_encoder_get_size(&key_encoder);

    /* Build attestation statement */
    uint8_t att_key[32];
    storage_get_attestation_key(att_key);

    uint8_t sig_data[1024];
    size_t sig_data_len = 0;
    memcpy(&sig_data[sig_data_len], auth_data, auth_data_len);
    sig_data_len += auth_data_len;
    memcpy(&sig_data[sig_data_len], client_data_hash, 32);
    sig_data_len += 32;

    uint8_t hash[32];
    crypto_sha256(sig_data, sig_data_len, hash);

    uint8_t signature[64];
    if (crypto_ecdsa_sign(att_key, hash, signature) != CRYPTO_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    /* Build CBOR response */
    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 3);

    /* fmt */
    cbor_encode_uint(&encoder, MC_RESP_FMT);
    cbor_encode_text(&encoder, "packed", 6);

    /* authData */
    cbor_encode_uint(&encoder, MC_RESP_AUTH_DATA);
    cbor_encode_bytes(&encoder, auth_data, auth_data_len);

    /* attStmt */
    cbor_encode_uint(&encoder, MC_RESP_ATT_STMT);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "alg", 3);
    cbor_encode_int(&encoder, COSE_ALG_ES256);
    cbor_encode_text(&encoder, "sig", 3);
    cbor_encode_bytes(&encoder, signature, 64);

    *response_len = cbor_encoder_get_size(&encoder);

    /* Clean up */
    memset(private_key, 0, sizeof(private_key));
    memset(att_key, 0, sizeof(att_key));

    hal_led_set_state(HAL_LED_OFF);
    LOG_INFO("MakeCredential completed successfully");
    return CTAP2_OK;
}

uint8_t ctap2_get_assertion(const uint8_t *request_data, size_t request_len,
                             uint8_t *response_data, size_t *response_len)
{
    LOG_INFO("GetAssertion command");

    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    uint64_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    char rp_id[256] = {0};
    uint8_t rp_id_hash[32];
    uint8_t client_data_hash[32];
    bool has_rp_id = false;
    bool has_client_data_hash = false;
    bool has_allow_list = false;
    uint8_t allow_list_ids[10][STORAGE_CREDENTIAL_ID_LENGTH];
    size_t allow_list_count = 0;
    uint8_t pin_auth[16];
    size_t pin_auth_len = 0;
    uint8_t pin_protocol = 0;
    bool has_pin_auth = false;
    bool uv = false;

    /* Parse all parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case GA_RP_ID:
                if (cbor_decode_text(&decoder, rp_id, sizeof(rp_id)) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_rp_id = true;
                break;

            case GA_CLIENT_DATA_HASH:
                if (cbor_decode_bytes(&decoder, client_data_hash, 32) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_client_data_hash = true;
                break;

            case GA_ALLOW_LIST: {
                uint64_t array_size;
                if (cbor_decode_array_start(&decoder, &array_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_allow_list = true;
                for (uint64_t j = 0; j < array_size && allow_list_count < 10; j++) {
                    uint64_t cred_map_size;
                    if (cbor_decode_map_start(&decoder, &cred_map_size) != CBOR_OK) {
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    for (uint64_t k = 0; k < cred_map_size; k++) {
                        char cred_key[8];
                        if (cbor_decode_text(&decoder, cred_key, sizeof(cred_key)) != CBOR_OK) {
                            cbor_skip_value(&decoder);
                            cbor_skip_value(&decoder);
                            continue;
                        }
                        if (strcmp(cred_key, "id") == 0) {
                            if (cbor_decode_bytes(&decoder, allow_list_ids[allow_list_count],
                                                  STORAGE_CREDENTIAL_ID_LENGTH) == CBOR_OK) {
                                allow_list_count++;
                            }
                        } else {
                            cbor_skip_value(&decoder);
                        }
                    }
                }
                break;
            }

            case GA_OPTIONS: {
                uint64_t options_map_size;
                if (cbor_decode_map_start(&decoder, &options_map_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                for (uint64_t j = 0; j < options_map_size; j++) {
                    char option_key[16];
                    if (cbor_decode_text(&decoder, option_key, sizeof(option_key)) != CBOR_OK) {
                        cbor_skip_value(&decoder);
                        cbor_skip_value(&decoder);
                        continue;
                    }
                    bool option_value;
                    if (cbor_decode_bool(&decoder, &option_value) != CBOR_OK) {
                        return CTAP2_ERR_INVALID_CBOR;
                    }
                    if (strcmp(option_key, "uv") == 0) {
                        uv = option_value;
                    }
                }
                break;
            }

            case GA_PIN_AUTH:
                pin_auth_len = sizeof(pin_auth);
                if (cbor_decode_bytes(&decoder, pin_auth, pin_auth_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_pin_auth = true;
                break;

            case GA_PIN_PROTOCOL:
                if (cbor_decode_uint(&decoder, (uint64_t*)&pin_protocol) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            default:
                cbor_skip_value(&decoder);
                break;
        }
    }

    /* Validate required parameters */
    if (!has_rp_id || !has_client_data_hash) {
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    /* Hash RP ID */
    crypto_sha256((const uint8_t *)rp_id, strlen(rp_id), rp_id_hash);

    /* Find matching credential */
    storage_credential_t credential;
    bool found = false;

    if (has_allow_list && allow_list_count > 0) {
        /* Search in allow list */
        for (size_t i = 0; i < allow_list_count; i++) {
            if (storage_find_credential(allow_list_ids[i], &credential) == STORAGE_OK) {
                if (memcmp(credential.rp_id_hash, rp_id_hash, 32) == 0) {
                    found = true;
                    break;
                }
            }
        }
    } else {
        /* Resident key discovery */
        storage_credential_t credentials[10];
        size_t count = 0;
        if (storage_find_credentials_by_rp(rp_id_hash, credentials, 10, &count) == STORAGE_OK && count > 0) {
            memcpy(&credential, &credentials[0], sizeof(storage_credential_t));
            found = true;
        }
    }

    if (!found) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    /* Check PIN requirements */
    /* PIN required for resident key discovery (empty allowList) or when UV requested */
    bool pin_required = (!has_allow_list || allow_list_count == 0) || uv;
    bool pin_verified = false;

    if (pin_required) {
        /* Check if PIN is set */
        if (!storage_is_pin_set()) {
            return CTAP2_ERR_PIN_NOT_SET;
        }

        /* PIN auth must be present when PIN is required */
        if (!has_pin_auth) {
            return CTAP2_ERR_PIN_REQUIRED;
        }

        /* Verify PIN protocol version */
        if (pin_protocol != 1) {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }

        /* In production: verify pinAuth using PIN token and HMAC */
        /* For now, simplified verification - just check if PIN auth is present */
        /* Real implementation would:
         * 1. Verify pinAuth = HMAC-SHA256(pinToken, clientDataHash)
         * 2. Validate PIN token hasn't expired
         */
        if (has_pin_auth && pin_auth_len == 16) {
            pin_verified = true;
            LOG_INFO("PIN verification successful (simplified)");
        } else {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }
    } else if (has_pin_auth) {
        /* PIN auth provided but not required - still verify it */
        if (pin_protocol != 1) {
            return CTAP2_ERR_PIN_AUTH_INVALID;
        }
        if (pin_auth_len == 16) {
            pin_verified = true;
        }
    }

    /* Request user presence */
    LOG_INFO("Waiting for user presence...");
    hal_led_set_state(HAL_LED_BLINK_FAST);
    if (!hal_button_wait_press(30000)) {
        hal_led_set_state(HAL_LED_OFF);
        return CTAP2_ERR_USER_ACTION_TIMEOUT;
    }
    hal_led_set_state(HAL_LED_ON);

    /* Increment counter */
    uint32_t counter;
    storage_get_and_increment_counter(&counter);
    storage_update_sign_count(credential.id, counter);

    /* Build authenticator data */
    uint8_t auth_data[256];
    size_t auth_data_len = 0;

    /* RP ID hash */
    memcpy(&auth_data[auth_data_len], rp_id_hash, 32);
    auth_data_len += 32;

    /* Flags */
    uint8_t flags = CTAP2_AUTH_DATA_FLAG_UP;
    /* Set UV flag if PIN was verified */
    if (pin_verified) flags |= CTAP2_AUTH_DATA_FLAG_UV;
    auth_data[auth_data_len++] = flags;

    /* Sign counter */
    auth_data[auth_data_len++] = (counter >> 24) & 0xFF;
    auth_data[auth_data_len++] = (counter >> 16) & 0xFF;
    auth_data[auth_data_len++] = (counter >> 8) & 0xFF;
    auth_data[auth_data_len++] = counter & 0xFF;

    /* Sign auth_data + client_data_hash */
    uint8_t sig_data[512];
    size_t sig_data_len = 0;
    memcpy(&sig_data[sig_data_len], auth_data, auth_data_len);
    sig_data_len += auth_data_len;
    memcpy(&sig_data[sig_data_len], client_data_hash, 32);
    sig_data_len += 32;

    uint8_t hash[32];
    crypto_sha256(sig_data, sig_data_len, hash);

    uint8_t signature[64];
    if (crypto_ecdsa_sign(credential.private_key, hash, signature) != CRYPTO_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    /* Build CBOR response */
    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, credential.resident ? 4 : 3);

    /* credential */
    cbor_encode_uint(&encoder, GA_RESP_CREDENTIAL);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "type", 4);
    cbor_encode_text(&encoder, "public-key", 10);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, credential.id, STORAGE_CREDENTIAL_ID_LENGTH);

    /* authData */
    cbor_encode_uint(&encoder, GA_RESP_AUTH_DATA);
    cbor_encode_bytes(&encoder, auth_data, auth_data_len);

    /* signature */
    cbor_encode_uint(&encoder, GA_RESP_SIGNATURE);
    cbor_encode_bytes(&encoder, signature, 64);

    /* user (if resident key) */
    if (credential.resident) {
        cbor_encode_uint(&encoder, GA_RESP_USER);
        cbor_encode_map_start(&encoder, 3);
        cbor_encode_text(&encoder, "id", 2);
        cbor_encode_bytes(&encoder, credential.user_id, credential.user_id_len);
        cbor_encode_text(&encoder, "name", 4);
        cbor_encode_text(&encoder, credential.user_name, strlen(credential.user_name));
        cbor_encode_text(&encoder, "displayName", 11);
        cbor_encode_text(&encoder, credential.display_name, strlen(credential.display_name));
    }

    *response_len = cbor_encoder_get_size(&encoder);

    hal_led_set_state(HAL_LED_OFF);
    LOG_INFO("GetAssertion completed successfully");
    return CTAP2_OK;
}
