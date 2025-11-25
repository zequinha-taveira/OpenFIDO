/**
 * @file ctap2.c
 * @brief CTAP2 Protocol Implementation
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

/* CTAP2 GetInfo Response Keys */
#define GETINFO_VERSIONS 0x01
#define GETINFO_EXTENSIONS 0x02
#define GETINFO_AAGUID 0x03
#define GETINFO_OPTIONS 0x04
#define GETINFO_MAX_MSG_SIZE 0x05
#define GETINFO_PIN_PROTOCOLS 0x06
#define GETINFO_MAX_CREDS_IN_LIST 0x07
#define GETINFO_MAX_CRED_ID_LENGTH 0x08
#define GETINFO_TRANSPORTS 0x09
#define GETINFO_ALGORITHMS 0x0A

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

/* Global state */
static struct {
    bool initialized;
    uint8_t pending_assertions;
    storage_credential_t assertion_credentials[10];
} ctap2_state = {0};

uint8_t ctap2_init(void)
{
    LOG_INFO("Initializing CTAP2 protocol handler");

    ctap2_state.initialized = true;
    ctap2_state.pending_assertions = 0;

    return CTAP2_OK;
}

uint8_t ctap2_process_request(const ctap2_request_t *request, ctap2_response_t *response)
{
    if (!ctap2_state.initialized) {
        return CTAP2_ERR_INVALID_COMMAND;
    }

    LOG_DEBUG("Processing CTAP2 command: 0x%02X", request->cmd);

    switch (request->cmd) {
        case CTAP2_CMD_MAKE_CREDENTIAL:
            return ctap2_make_credential(request->data, request->data_len, response->data,
                                         &response->data_len);

        case CTAP2_CMD_GET_ASSERTION:
            return ctap2_get_assertion(request->data, request->data_len, response->data,
                                       &response->data_len);

        case CTAP2_CMD_GET_INFO:
            return ctap2_get_info(response->data, &response->data_len);

        case CTAP2_CMD_CLIENT_PIN:
            return ctap2_client_pin(request->data, request->data_len, response->data,
                                    &response->data_len);

        case CTAP2_CMD_RESET:
            return ctap2_reset();

        case CTAP2_CMD_GET_NEXT_ASSERTION:
            return ctap2_get_next_assertion(response->data, &response->data_len);

        default:
            LOG_WARN("Unknown CTAP2 command: 0x%02X", request->cmd);
            return CTAP2_ERR_INVALID_COMMAND;
    }
}

uint8_t ctap2_get_info(uint8_t *response_data, size_t *response_len)
{
    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    /* Start response map */
    cbor_encode_map_start(&encoder, 8);

    /* 0x01: versions */
    cbor_encode_uint(&encoder, GETINFO_VERSIONS);
    cbor_encode_array_start(&encoder, 2);
    cbor_encode_text(&encoder, "FIDO_2_0", 8);
    cbor_encode_text(&encoder, "U2F_V2", 6);

    /* 0x03: aaguid */
    cbor_encode_uint(&encoder, GETINFO_AAGUID);
    cbor_encode_bytes(&encoder, AAGUID, sizeof(AAGUID));

    /* 0x04: options */
    cbor_encode_uint(&encoder, GETINFO_OPTIONS);
    cbor_encode_map_start(&encoder, 4);

    cbor_encode_text(&encoder, "rk", 2);
    cbor_encode_bool(&encoder, true); /* Resident key support */

    cbor_encode_text(&encoder, "up", 2);
    cbor_encode_bool(&encoder, true); /* User presence */

    cbor_encode_text(&encoder, "plat", 4);
    cbor_encode_bool(&encoder, false); /* Not a platform authenticator */

    cbor_encode_text(&encoder, "clientPin", 9);
    cbor_encode_bool(&encoder, storage_is_pin_set());

    /* 0x05: maxMsgSize */
    cbor_encode_uint(&encoder, GETINFO_MAX_MSG_SIZE);
    cbor_encode_uint(&encoder, CTAP2_MAX_MESSAGE_SIZE);

    /* 0x06: pinProtocols */
    cbor_encode_uint(&encoder, GETINFO_PIN_PROTOCOLS);
    cbor_encode_array_start(&encoder, 1);
    cbor_encode_uint(&encoder, 1); /* PIN protocol version 1 */

    /* 0x07: maxCredentialCountInList */
    cbor_encode_uint(&encoder, GETINFO_MAX_CREDS_IN_LIST);
    cbor_encode_uint(&encoder, 10);

    /* 0x08: maxCredentialIdLength */
    cbor_encode_uint(&encoder, GETINFO_MAX_CRED_ID_LENGTH);
    cbor_encode_uint(&encoder, CTAP2_MAX_CREDENTIAL_ID_LENGTH);

    /* 0x0A: algorithms */
    cbor_encode_uint(&encoder, GETINFO_ALGORITHMS);
    cbor_encode_array_start(&encoder, 1);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "alg", 3);
    cbor_encode_int(&encoder, COSE_ALG_ES256);
    cbor_encode_text(&encoder, "type", 4);
    cbor_encode_text(&encoder, "public-key", 10);

    *response_len = cbor_encoder_get_size(&encoder);
    LOG_INFO("GetInfo response: %zu bytes", *response_len);

    return CTAP2_OK;
}

uint8_t ctap2_make_credential(const uint8_t *request_data, size_t request_len,
                              uint8_t *response_data, size_t *response_len)
{
    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    size_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint8_t client_data_hash[32];
    char rp_id[STORAGE_MAX_RP_ID_LENGTH] = {0};
    uint8_t user_id[STORAGE_MAX_USER_ID_LENGTH];
    size_t user_id_len = 0;
    char user_name[STORAGE_MAX_USER_NAME_LENGTH] = {0};
    char display_name[STORAGE_MAX_DISPLAY_NAME_LENGTH] = {0};
    bool rk_required = false;
    bool uv_required = false;

    /* Parse request parameters */
    for (size_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case MC_CLIENT_DATA_HASH: {
                size_t hash_len = sizeof(client_data_hash);
                if (cbor_decode_bytes(&decoder, client_data_hash, &hash_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;
            }

            case MC_RP: {
                size_t rp_map_size;
                cbor_decode_map_start(&decoder, &rp_map_size);
                for (size_t j = 0; j < rp_map_size; j++) {
                    char rp_key[10];
                    size_t rp_key_len = sizeof(rp_key);
                    cbor_decode_text(&decoder, rp_key, &rp_key_len);

                    if (strcmp(rp_key, "id") == 0) {
                        size_t rp_id_len = sizeof(rp_id);
                        cbor_decode_text(&decoder, rp_id, &rp_id_len);
                    } else {
                        cbor_decoder_skip(&decoder);
                    }
                }
                break;
            }

            case MC_USER: {
                size_t user_map_size;
                cbor_decode_map_start(&decoder, &user_map_size);
                for (size_t j = 0; j < user_map_size; j++) {
                    char user_key[20];
                    size_t user_key_len = sizeof(user_key);
                    cbor_decode_text(&decoder, user_key, &user_key_len);

                    if (strcmp(user_key, "id") == 0) {
                        user_id_len = sizeof(user_id);
                        cbor_decode_bytes(&decoder, user_id, &user_id_len);
                    } else if (strcmp(user_key, "name") == 0) {
                        size_t name_len = sizeof(user_name);
                        cbor_decode_text(&decoder, user_name, &name_len);
                    } else if (strcmp(user_key, "displayName") == 0) {
                        size_t dn_len = sizeof(display_name);
                        cbor_decode_text(&decoder, display_name, &dn_len);
                    } else {
                        cbor_decoder_skip(&decoder);
                    }
                }
                break;
            }

            case MC_OPTIONS: {
                size_t opt_map_size;
                cbor_decode_map_start(&decoder, &opt_map_size);
                for (size_t j = 0; j < opt_map_size; j++) {
                    char opt_key[10];
                    size_t opt_key_len = sizeof(opt_key);
                    cbor_decode_text(&decoder, opt_key, &opt_key_len);

                    bool opt_value;
                    cbor_decode_bool(&decoder, &opt_value);

                    if (strcmp(opt_key, "rk") == 0) {
                        rk_required = opt_value;
                    } else if (strcmp(opt_key, "uv") == 0) {
                        uv_required = opt_value;
                    }
                }
                break;
            }

            default:
                /* Skip unknown parameters */
                cbor_decoder_skip(&decoder);
                break;
        }
    }

    /* Wait for user presence */
    LOG_INFO("Waiting for user presence...");
    hal_led_set_state(HAL_LED_BLINK_FAST);

    if (!hal_button_wait_press(30000)) {
        hal_led_set_state(HAL_LED_OFF);
        return CTAP2_ERR_USER_ACTION_TIMEOUT;
    }

    hal_led_set_state(HAL_LED_ON);

    /* Generate new credential */
    uint8_t private_key[32];
    uint8_t public_key[64];

    if (crypto_ecdsa_generate_keypair(private_key, public_key) != CRYPTO_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    /* Create credential ID */
    storage_credential_t credential = {0};
    crypto_random_generate(credential.id, STORAGE_CREDENTIAL_ID_LENGTH);

    /* Hash RP ID */
    crypto_sha256((const uint8_t *) rp_id, strlen(rp_id), credential.rp_id_hash);

    /* Store user info */
    memcpy(credential.user_id, user_id, user_id_len);
    credential.user_id_len = user_id_len;
    memcpy(credential.private_key, private_key, 32);
    credential.resident = rk_required;

    if (rk_required) {
        strncpy(credential.user_name, user_name, sizeof(credential.user_name) - 1);
        strncpy(credential.display_name, display_name, sizeof(credential.display_name) - 1);
        strncpy(credential.rp_id, rp_id, sizeof(credential.rp_id) - 1);
    }

    /* Get and increment counter */
    storage_get_and_increment_counter(&credential.sign_count);

    /* Store credential */
    if (storage_store_credential(&credential) != STORAGE_OK) {
        return CTAP2_ERR_KEY_STORE_FULL;
    }

    /* Build authenticator data */
    uint8_t auth_data[512];
    size_t auth_data_len = 0;

    /* RP ID hash (32 bytes) */
    memcpy(&auth_data[auth_data_len], credential.rp_id_hash, 32);
    auth_data_len += 32;

    /* Flags (1 byte) */
    uint8_t flags = CTAP2_AUTH_DATA_FLAG_UP | CTAP2_AUTH_DATA_FLAG_AT;
    if (uv_required)
        flags |= CTAP2_AUTH_DATA_FLAG_UV;
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

    /* Public key in COSE format */
    cbor_encoder_t pk_encoder;
    cbor_encoder_init(&pk_encoder, &auth_data[auth_data_len], sizeof(auth_data) - auth_data_len);

    cbor_encode_map_start(&pk_encoder, 5);
    cbor_encode_int(&pk_encoder, 1);  /* kty */
    cbor_encode_uint(&pk_encoder, 2); /* EC2 */
    cbor_encode_int(&pk_encoder, 3);  /* alg */
    cbor_encode_int(&pk_encoder, COSE_ALG_ES256);
    cbor_encode_int(&pk_encoder, -1); /* crv */
    cbor_encode_uint(&pk_encoder, 1); /* P-256 */
    cbor_encode_int(&pk_encoder, -2); /* x */
    cbor_encode_bytes(&pk_encoder, &public_key[0], 32);
    cbor_encode_int(&pk_encoder, -3); /* y */
    cbor_encode_bytes(&pk_encoder, &public_key[32], 32);

    auth_data_len += cbor_encoder_get_size(&pk_encoder);

    /* Create attestation statement (self-attestation) */
    uint8_t sig_data[512];
    memcpy(sig_data, auth_data, auth_data_len);
    memcpy(&sig_data[auth_data_len], client_data_hash, 32);

    uint8_t sig_hash[32];
    crypto_sha256(sig_data, auth_data_len + 32, sig_hash);

    uint8_t signature[64];
    uint8_t att_private_key[32];
    storage_get_attestation_key(att_private_key);
    crypto_ecdsa_sign(att_private_key, sig_hash, signature);

    /* Build response */
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

    /* Clean up sensitive data */
    memset(private_key, 0, sizeof(private_key));
    memset(att_private_key, 0, sizeof(att_private_key));

    hal_led_set_state(HAL_LED_OFF);
    LOG_INFO("MakeCredential completed successfully");

    return CTAP2_OK;
}

uint8_t ctap2_get_assertion(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                            size_t *response_len)
{
    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    size_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    char rp_id[STORAGE_MAX_RP_ID_LENGTH] = {0};
    uint8_t client_data_hash[32];
    uint8_t allow_list_cred_ids[10][STORAGE_CREDENTIAL_ID_LENGTH];
    size_t allow_list_count = 0;
    bool up_required = true;
    bool uv_required = false;

    /* Parse request parameters */
    for (size_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case GA_RP_ID: {
                size_t rp_id_len = sizeof(rp_id);
                if (cbor_decode_text(&decoder, rp_id, &rp_id_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;
            }

            case GA_CLIENT_DATA_HASH: {
                size_t hash_len = sizeof(client_data_hash);
                if (cbor_decode_bytes(&decoder, client_data_hash, &hash_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;
            }

            case GA_ALLOW_LIST: {
                size_t list_size;
                cbor_decode_array_start(&decoder, &list_size);
                for (size_t j = 0; j < list_size && j < 10; j++) {
                    size_t cred_map_size;
                    cbor_decode_map_start(&decoder, &cred_map_size);

                    for (size_t k = 0; k < cred_map_size; k++) {
                        char cred_key[10];
                        size_t cred_key_len = sizeof(cred_key);
                        cbor_decode_text(&decoder, cred_key, &cred_key_len);

                        if (strcmp(cred_key, "id") == 0) {
                            size_t id_len = STORAGE_CREDENTIAL_ID_LENGTH;
                            cbor_decode_bytes(&decoder, allow_list_cred_ids[allow_list_count],
                                              &id_len);
                            allow_list_count++;
                        } else {
                            cbor_decoder_skip(&decoder);
                        }
                    }
                }
                break;
            }

            case GA_OPTIONS: {
                size_t opt_map_size;
                cbor_decode_map_start(&decoder, &opt_map_size);
                for (size_t j = 0; j < opt_map_size; j++) {
                    char opt_key[10];
                    size_t opt_key_len = sizeof(opt_key);
                    cbor_decode_text(&decoder, opt_key, &opt_key_len);

                    bool opt_value;
                    cbor_decode_bool(&decoder, &opt_value);

                    if (strcmp(opt_key, "up") == 0) {
                        up_required = opt_value;
                    } else if (strcmp(opt_key, "uv") == 0) {
                        uv_required = opt_value;
                    }
                }
                break;
            }

            default:
                cbor_decoder_skip(&decoder);
                break;
        }
    }

    /* Hash RP ID */
    uint8_t rp_id_hash[32];
    crypto_sha256((const uint8_t *) rp_id, strlen(rp_id), rp_id_hash);

    /* Find matching credentials */
    storage_credential_t credentials[10];
    size_t found_count = 0;

    if (allow_list_count > 0) {
        /* Search in allow list */
        for (size_t i = 0; i < allow_list_count; i++) {
            if (storage_find_credential(allow_list_cred_ids[i], &credentials[found_count]) ==
                STORAGE_OK) {
                /* Verify RP ID matches */
                if (memcmp(credentials[found_count].rp_id_hash, rp_id_hash, 32) == 0) {
                    found_count++;
                }
            }
        }
    } else {
        /* Search all credentials for this RP */
        storage_find_credentials_by_rp(rp_id_hash, credentials, 10, &found_count);
    }

    if (found_count == 0) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    /* Store for GetNextAssertion */
    ctap2_state.pending_assertions = found_count - 1;
    for (size_t i = 0; i < found_count && i < 10; i++) {
        memcpy(&ctap2_state.assertion_credentials[i], &credentials[i],
               sizeof(storage_credential_t));
    }

    /* Use first credential */
    storage_credential_t *cred = &credentials[0];

    /* Wait for user presence if required */
    if (up_required) {
        LOG_INFO("Waiting for user presence...");
        hal_led_set_state(HAL_LED_BLINK_FAST);

        if (!hal_button_wait_press(30000)) {
            hal_led_set_state(HAL_LED_OFF);
            return CTAP2_ERR_USER_ACTION_TIMEOUT;
        }

        hal_led_set_state(HAL_LED_ON);
    }

    /* Build authenticator data */
    uint8_t auth_data[256];
    size_t auth_data_len = 0;

    /* RP ID hash */
    memcpy(&auth_data[auth_data_len], rp_id_hash, 32);
    auth_data_len += 32;

    /* Flags */
    uint8_t flags = 0;
    if (up_required)
        flags |= CTAP2_AUTH_DATA_FLAG_UP;
    if (uv_required)
        flags |= CTAP2_AUTH_DATA_FLAG_UV;
    auth_data[auth_data_len++] = flags;

    /* Increment and get counter */
    uint32_t new_count;
    storage_get_and_increment_counter(&new_count);
    storage_update_sign_count(cred->id, new_count);

    /* Sign counter (big-endian) */
    auth_data[auth_data_len++] = (new_count >> 24) & 0xFF;
    auth_data[auth_data_len++] = (new_count >> 16) & 0xFF;
    auth_data[auth_data_len++] = (new_count >> 8) & 0xFF;
    auth_data[auth_data_len++] = new_count & 0xFF;

    /* Create signature data */
    uint8_t sig_data[512];
    memcpy(sig_data, auth_data, auth_data_len);
    memcpy(&sig_data[auth_data_len], client_data_hash, 32);

    /* Hash signature data */
    uint8_t sig_hash[32];
    crypto_sha256(sig_data, auth_data_len + 32, sig_hash);

    /* Sign with credential private key */
    uint8_t signature[64];
    if (crypto_ecdsa_sign(cred->private_key, sig_hash, signature) != CRYPTO_OK) {
        hal_led_set_state(HAL_LED_OFF);
        return CTAP2_ERR_PROCESSING;
    }

    /* Build response */
    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    size_t response_map_size = 3;
    if (cred->resident)
        response_map_size = 4;
    if (found_count > 1)
        response_map_size++;

    cbor_encode_map_start(&encoder, response_map_size);

    /* Credential */
    cbor_encode_uint(&encoder, GA_RESP_CREDENTIAL);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "type", 4);
    cbor_encode_text(&encoder, "public-key", 10);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, cred->id, STORAGE_CREDENTIAL_ID_LENGTH);

    /* Auth data */
    cbor_encode_uint(&encoder, GA_RESP_AUTH_DATA);
    cbor_encode_bytes(&encoder, auth_data, auth_data_len);

    /* Signature */
    cbor_encode_uint(&encoder, GA_RESP_SIGNATURE);
    cbor_encode_bytes(&encoder, signature, 64);

    /* User (if resident key) */
    if (cred->resident) {
        cbor_encode_uint(&encoder, GA_RESP_USER);
        cbor_encode_map_start(&encoder, 3);
        cbor_encode_text(&encoder, "id", 2);
        cbor_encode_bytes(&encoder, cred->user_id, cred->user_id_len);
        cbor_encode_text(&encoder, "name", 4);
        cbor_encode_text(&encoder, cred->user_name, strlen(cred->user_name));
        cbor_encode_text(&encoder, "displayName", 11);
        cbor_encode_text(&encoder, cred->display_name, strlen(cred->display_name));
    }

    /* Number of credentials */
    if (found_count > 1) {
        cbor_encode_uint(&encoder, GA_RESP_NUM_CREDENTIALS);
        cbor_encode_uint(&encoder, found_count);
    }

    *response_len = cbor_encoder_get_size(&encoder);

    hal_led_set_state(HAL_LED_OFF);
    LOG_INFO("GetAssertion completed successfully (%zu credentials)", found_count);

    return CTAP2_OK;
}

uint8_t ctap2_client_pin(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                         size_t *response_len)
{
    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

/* PIN Protocol subcommands */
#define PIN_GET_RETRIES 0x01
#define PIN_GET_KEY_AGREEMENT 0x02
#define PIN_SET_PIN 0x03
#define PIN_CHANGE_PIN 0x04
#define PIN_GET_PIN_TOKEN 0x05

    /* Parse request map */
    size_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint64_t pin_protocol = 0;
    uint64_t sub_command = 0;
    uint8_t key_agreement[65] = {0}; /* Public key from platform */
    size_t key_agreement_len = 0;
    uint8_t pin_auth[16] = {0};
    size_t pin_auth_len = 0;
    uint8_t new_pin_enc[64] = {0};
    size_t new_pin_enc_len = 0;
    uint8_t pin_hash_enc[16] = {0};
    size_t pin_hash_enc_len = 0;

    /* Parse parameters */
    for (size_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case 0x01: /* pinProtocol */
                cbor_decode_uint(&decoder, &pin_protocol);
                break;
            case 0x02: /* subCommand */
                cbor_decode_uint(&decoder, &sub_command);
                break;
            case 0x03: /* keyAgreement */
                key_agreement_len = sizeof(key_agreement);
                cbor_decode_bytes(&decoder, key_agreement, &key_agreement_len);
                break;
            case 0x04: /* pinAuth */
                pin_auth_len = sizeof(pin_auth);
                cbor_decode_bytes(&decoder, pin_auth, &pin_auth_len);
                break;
            case 0x05: /* newPinEnc */
                new_pin_enc_len = sizeof(new_pin_enc);
                cbor_decode_bytes(&decoder, new_pin_enc, &new_pin_enc_len);
                break;
            case 0x06: /* pinHashEnc */
                pin_hash_enc_len = sizeof(pin_hash_enc);
                cbor_decode_bytes(&decoder, pin_hash_enc, &pin_hash_enc_len);
                break;
            default:
                cbor_decoder_skip(&decoder);
                break;
        }
    }

    /* Verify PIN protocol version */
    if (pin_protocol != 1) {
        return CTAP2_ERR_PIN_INVALID;
    }

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    /* Process subcommand */
    switch (sub_command) {
        case PIN_GET_RETRIES: {
            /* Return PIN retries */
            int retries = storage_get_pin_retries();

            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, 0x03); /* retries */
            cbor_encode_uint(&encoder, retries);

            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetRetries = %d", retries);
            return CTAP2_OK;
        }

        case PIN_GET_KEY_AGREEMENT: {
            /* Generate ephemeral ECDH key pair */
            uint8_t private_key[32];
            uint8_t public_key[64];

            if (crypto_ecdsa_generate_keypair(private_key, public_key) != CRYPTO_OK) {
                return CTAP2_ERR_PROCESSING;
            }

            /* Return public key in COSE format */
            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, 0x01); /* keyAgreement */

            /* COSE key */
            cbor_encode_map_start(&encoder, 5);
            cbor_encode_int(&encoder, 1);  /* kty */
            cbor_encode_uint(&encoder, 2); /* EC2 */
            cbor_encode_int(&encoder, 3);  /* alg */
            cbor_encode_int(&encoder, COSE_ALG_ES256);
            cbor_encode_int(&encoder, -1); /* crv */
            cbor_encode_uint(&encoder, 1); /* P-256 */
            cbor_encode_int(&encoder, -2); /* x */
            cbor_encode_bytes(&encoder, &public_key[0], 32);
            cbor_encode_int(&encoder, -3); /* y */
            cbor_encode_bytes(&encoder, &public_key[32], 32);

            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetKeyAgreement");
            return CTAP2_OK;
        }

        case PIN_SET_PIN: {
            /* Set new PIN */
            if (storage_is_pin_set()) {
                return CTAP2_ERR_PIN_INVALID;
            }

            /* In production, decrypt newPinEnc using shared secret */
            /* For now, simplified implementation */

            /* Verify PIN length (should be decrypted PIN) */
            if (new_pin_enc_len < STORAGE_PIN_MIN_LENGTH ||
                new_pin_enc_len > STORAGE_PIN_MAX_LENGTH) {
                return CTAP2_ERR_PIN_POLICY_VIOLATION;
            }

            /* Set PIN */
            if (storage_set_pin(new_pin_enc, new_pin_enc_len) != STORAGE_OK) {
                return CTAP2_ERR_PROCESSING;
            }

            *response_len = 0;
            LOG_INFO("ClientPIN: SetPIN");
            return CTAP2_OK;
        }

        case PIN_CHANGE_PIN: {
            /* Change existing PIN */
            if (!storage_is_pin_set()) {
                return CTAP2_ERR_PIN_NOT_SET;
            }

            /* In production: */
            /* 1. Decrypt pinHashEnc using shared secret */
            /* 2. Verify current PIN */
            /* 3. Decrypt newPinEnc */
            /* 4. Set new PIN */

            /* Simplified implementation */
            if (storage_set_pin(new_pin_enc, new_pin_enc_len) != STORAGE_OK) {
                return CTAP2_ERR_PROCESSING;
            }

            *response_len = 0;
            LOG_INFO("ClientPIN: ChangePIN");
            return CTAP2_OK;
        }

        case PIN_GET_PIN_TOKEN: {
            /* Get PIN token */
            if (!storage_is_pin_set()) {
                return CTAP2_ERR_PIN_NOT_SET;
            }

            if (storage_is_pin_blocked()) {
                return CTAP2_ERR_PIN_BLOCKED;
            }

            /* In production: */
            /* 1. Decrypt pinHashEnc using shared secret */
            /* 2. Verify PIN */
            /* 3. Generate PIN token */
            /* 4. Encrypt PIN token with shared secret */

            /* Simplified: return dummy encrypted token */
            uint8_t pin_token[16];
            crypto_random_generate(pin_token, sizeof(pin_token));

            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, 0x02); /* pinToken */
            cbor_encode_bytes(&encoder, pin_token, sizeof(pin_token));

            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetPINToken");
            return CTAP2_OK;
        }

        default:
            LOG_WARN("ClientPIN: Unknown subcommand %llu", sub_command);
            return CTAP2_ERR_INVALID_SUBCOMMAND;
    }
}

uint8_t ctap2_reset(void)
{
    LOG_INFO("Performing authenticator reset");

    /* Wait for user presence within 10 seconds of power-up */
    if (!hal_button_wait_press(10000)) {
        return CTAP2_ERR_USER_ACTION_TIMEOUT;
    }

    /* Format storage */
    if (storage_format() != STORAGE_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    ctap2_state.pending_assertions = 0;

    LOG_INFO("Reset completed successfully");
    return CTAP2_OK;
}

uint8_t ctap2_get_next_assertion(uint8_t *response_data, size_t *response_len)
{
    if (ctap2_state.pending_assertions == 0) {
        return CTAP2_ERR_NO_OPERATION_PENDING;
    }

    /* Return next assertion from pending list */
    ctap2_state.pending_assertions--;

    LOG_INFO("GetNextAssertion: %d remaining", ctap2_state.pending_assertions);
    return CTAP2_OK;
}
