/**
 * @file ctap2_credential_mgmt.c
 * @brief CTAP2 Credential Management Command Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <string.h>

#include "cbor.h"
#include "crypto.h"
#include "ctap2.h"
#include "logger.h"
#include "storage.h"

/* Credential Management Subcommands */
#define CM_GET_CREDS_METADATA 0x01
#define CM_ENUMERATE_RPS_BEGIN 0x02
#define CM_ENUMERATE_RPS_GET_NEXT 0x03
#define CM_ENUMERATE_CREDS_BEGIN 0x04
#define CM_ENUMERATE_CREDS_GET_NEXT 0x05
#define CM_DELETE_CREDENTIAL 0x06

/* Request Parameters */
#define CM_PARAM_SUBCOMMAND 0x01
#define CM_PARAM_SUBCOMMAND_PARAMS 0x02
#define CM_PARAM_PIN_PROTOCOL 0x03
#define CM_PARAM_PIN_AUTH 0x04

/* Response Parameters */
#define CM_RESP_EXISTING_CREDS 0x01
#define CM_RESP_MAX_POSSIBLE_CREDS 0x02
#define CM_RESP_RP 0x03
#define CM_RESP_RP_ID_HASH 0x04
#define CM_RESP_TOTAL_RPS 0x05
#define CM_RESP_USER 0x06
#define CM_RESP_CREDENTIAL_ID 0x07
#define CM_RESP_PUBLIC_KEY 0x08
#define CM_RESP_TOTAL_CREDENTIALS 0x09
#define CM_RESP_CRED_PROTECT 0x0A
#define CM_RESP_LARGE_BLOB_KEY 0x0B

/* Global state for enumeration */
static struct {
    bool rp_enumeration_active;
    bool cred_enumeration_active;
    size_t current_rp_index;
    size_t current_cred_index;
    uint8_t current_rp_id_hash[32];
    storage_credential_t enumerated_rps[STORAGE_MAX_CREDENTIALS];
    size_t total_rps;
    storage_credential_t enumerated_creds[STORAGE_MAX_CREDENTIALS];
    size_t total_creds;
} cm_state = {0};

/**
 * @brief Verify PIN authentication for credential management
 */
static uint8_t verify_cm_pin_auth(const uint8_t *pin_auth, size_t pin_auth_len,
                                  uint8_t pin_protocol)
{
    /* PIN is always required for credential management */
    if (!storage_is_pin_set()) {
        return CTAP2_ERR_PIN_NOT_SET;
    }

    if (pin_auth == NULL || pin_auth_len == 0) {
        return CTAP2_ERR_PIN_REQUIRED;
    }

    if (pin_protocol != 1) {
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    /* Simplified verification - check length */
    /* Production: verify HMAC-SHA256(pinToken, subCommand || subCommandParams) */
    if (pin_auth_len != 16) {
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    LOG_INFO("Credential management PIN verification successful (simplified)");
    return CTAP2_OK;
}

/**
 * @brief Get credentials metadata
 */
static uint8_t cm_get_metadata(uint8_t *response_data, size_t *response_len)
{
    size_t existing_creds = 0;
    size_t resident_creds = 0;

    if (storage_get_credential_count(&existing_creds) != STORAGE_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    if (storage_get_resident_credential_count(&resident_creds) != STORAGE_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 2);

    /* existingResidentCredentialsCount */
    cbor_encode_uint(&encoder, CM_RESP_EXISTING_CREDS);
    cbor_encode_uint(&encoder, resident_creds);

    /* maxPossibleRemainingResidentCredentialsCount */
    cbor_encode_uint(&encoder, CM_RESP_MAX_POSSIBLE_CREDS);
    cbor_encode_uint(&encoder, STORAGE_MAX_CREDENTIALS - existing_creds);

    *response_len = cbor_encoder_get_size(&encoder);

    LOG_INFO("Credential metadata: %zu resident, %zu total, %zu remaining", resident_creds,
             existing_creds, STORAGE_MAX_CREDENTIALS - existing_creds);

    return CTAP2_OK;
}

/**
 * @brief Begin RP enumeration
 */
static uint8_t cm_enumerate_rps_begin(uint8_t *response_data, size_t *response_len)
{
    /* Reset enumeration state */
    memset(&cm_state, 0, sizeof(cm_state));

    /* Get all credentials and extract unique RPs */
    storage_credential_t all_creds[STORAGE_MAX_CREDENTIALS];
    size_t total_creds = 0;

    /* Iterate through all credential slots */
    for (size_t i = 0; i < STORAGE_MAX_CREDENTIALS; i++) {
        storage_credential_t cred;
        uint8_t dummy_id[STORAGE_CREDENTIAL_ID_LENGTH];
        memset(dummy_id, i, sizeof(dummy_id)); /* Generate dummy ID for iteration */

        if (storage_find_credential(dummy_id, &cred) == STORAGE_OK && cred.resident) {
            /* Check if this RP is already in our list */
            bool found = false;
            for (size_t j = 0; j < cm_state.total_rps; j++) {
                if (memcmp(cm_state.enumerated_rps[j].rp_id_hash, cred.rp_id_hash, 32) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found && cm_state.total_rps < STORAGE_MAX_CREDENTIALS) {
                memcpy(&cm_state.enumerated_rps[cm_state.total_rps], &cred,
                       sizeof(storage_credential_t));
                cm_state.total_rps++;
            }
        }
    }

    if (cm_state.total_rps == 0) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    /* Return first RP */
    storage_credential_t *rp = &cm_state.enumerated_rps[0];

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 3);

    /* rp */
    cbor_encode_uint(&encoder, CM_RESP_RP);
    cbor_encode_map_start(&encoder, 1);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_text(&encoder, rp->rp_id, strlen(rp->rp_id));

    /* rpIDHash */
    cbor_encode_uint(&encoder, CM_RESP_RP_ID_HASH);
    cbor_encode_bytes(&encoder, rp->rp_id_hash, 32);

    /* totalRPs */
    cbor_encode_uint(&encoder, CM_RESP_TOTAL_RPS);
    cbor_encode_uint(&encoder, cm_state.total_rps);

    *response_len = cbor_encoder_get_size(&encoder);

    cm_state.rp_enumeration_active = true;
    cm_state.current_rp_index = 1;

    LOG_INFO("RP enumeration started: %zu total RPs", cm_state.total_rps);

    return CTAP2_OK;
}

/**
 * @brief Get next RP in enumeration
 */
static uint8_t cm_enumerate_rps_get_next(uint8_t *response_data, size_t *response_len)
{
    if (!cm_state.rp_enumeration_active) {
        return CTAP2_ERR_NO_OPERATION_PENDING;
    }

    if (cm_state.current_rp_index >= cm_state.total_rps) {
        cm_state.rp_enumeration_active = false;
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    storage_credential_t *rp = &cm_state.enumerated_rps[cm_state.current_rp_index];

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 2);

    /* rp */
    cbor_encode_uint(&encoder, CM_RESP_RP);
    cbor_encode_map_start(&encoder, 1);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_text(&encoder, rp->rp_id, strlen(rp->rp_id));

    /* rpIDHash */
    cbor_encode_uint(&encoder, CM_RESP_RP_ID_HASH);
    cbor_encode_bytes(&encoder, rp->rp_id_hash, 32);

    *response_len = cbor_encoder_get_size(&encoder);

    cm_state.current_rp_index++;

    if (cm_state.current_rp_index >= cm_state.total_rps) {
        cm_state.rp_enumeration_active = false;
    }

    return CTAP2_OK;
}

/**
 * @brief Begin credential enumeration for an RP
 */
static uint8_t cm_enumerate_creds_begin(const uint8_t *rp_id_hash, uint8_t *response_data,
                                        size_t *response_len)
{
    /* Reset credential enumeration state */
    cm_state.cred_enumeration_active = false;
    cm_state.current_cred_index = 0;
    cm_state.total_creds = 0;
    memcpy(cm_state.current_rp_id_hash, rp_id_hash, 32);

    /* Find all credentials for this RP */
    if (storage_find_credentials_by_rp(rp_id_hash, cm_state.enumerated_creds,
                                       STORAGE_MAX_CREDENTIALS,
                                       &cm_state.total_creds) != STORAGE_OK) {
        return CTAP2_ERR_PROCESSING;
    }

    if (cm_state.total_creds == 0) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    /* Return first credential */
    storage_credential_t *cred = &cm_state.enumerated_creds[0];

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 4);

    /* user */
    cbor_encode_uint(&encoder, CM_RESP_USER);
    cbor_encode_map_start(&encoder, 3);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, cred->user_id, cred->user_id_len);
    cbor_encode_text(&encoder, "name", 4);
    cbor_encode_text(&encoder, cred->user_name, strlen(cred->user_name));
    cbor_encode_text(&encoder, "displayName", 11);
    cbor_encode_text(&encoder, cred->display_name, strlen(cred->display_name));

    /* credentialID */
    cbor_encode_uint(&encoder, CM_RESP_CREDENTIAL_ID);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "type", 4);
    cbor_encode_text(&encoder, "public-key", 10);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, cred->id, STORAGE_CREDENTIAL_ID_LENGTH);

    /* publicKey (COSE format) */
    cbor_encode_uint(&encoder, CM_RESP_PUBLIC_KEY);
    uint8_t public_key[64];
    if (crypto_ecdsa_get_public_key(cred->private_key, public_key) == CRYPTO_OK) {
        cbor_encode_map_start(&encoder, 5);
        cbor_encode_int(&encoder, 1); /* kty */
        cbor_encode_int(&encoder, 2); /* EC2 */
        cbor_encode_int(&encoder, 3); /* alg */
        cbor_encode_int(&encoder, cred->algorithm);
        cbor_encode_int(&encoder, -1); /* crv */
        cbor_encode_int(&encoder, 1);  /* P-256 */
        cbor_encode_int(&encoder, -2); /* x */
        cbor_encode_bytes(&encoder, &public_key[0], 32);
        cbor_encode_int(&encoder, -3); /* y */
        cbor_encode_bytes(&encoder, &public_key[32], 32);
    }

    /* totalCredentials */
    cbor_encode_uint(&encoder, CM_RESP_TOTAL_CREDENTIALS);
    cbor_encode_uint(&encoder, cm_state.total_creds);

    *response_len = cbor_encoder_get_size(&encoder);

    cm_state.cred_enumeration_active = true;
    cm_state.current_cred_index = 1;

    LOG_INFO("Credential enumeration started: %zu total credentials", cm_state.total_creds);

    return CTAP2_OK;
}

/**
 * @brief Get next credential in enumeration
 */
static uint8_t cm_enumerate_creds_get_next(uint8_t *response_data, size_t *response_len)
{
    if (!cm_state.cred_enumeration_active) {
        return CTAP2_ERR_NO_OPERATION_PENDING;
    }

    if (cm_state.current_cred_index >= cm_state.total_creds) {
        cm_state.cred_enumeration_active = false;
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    storage_credential_t *cred = &cm_state.enumerated_creds[cm_state.current_cred_index];

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 3);

    /* user */
    cbor_encode_uint(&encoder, CM_RESP_USER);
    cbor_encode_map_start(&encoder, 3);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, cred->user_id, cred->user_id_len);
    cbor_encode_text(&encoder, "name", 4);
    cbor_encode_text(&encoder, cred->user_name, strlen(cred->user_name));
    cbor_encode_text(&encoder, "displayName", 11);
    cbor_encode_text(&encoder, cred->display_name, strlen(cred->display_name));

    /* credentialID */
    cbor_encode_uint(&encoder, CM_RESP_CREDENTIAL_ID);
    cbor_encode_map_start(&encoder, 2);
    cbor_encode_text(&encoder, "type", 4);
    cbor_encode_text(&encoder, "public-key", 10);
    cbor_encode_text(&encoder, "id", 2);
    cbor_encode_bytes(&encoder, cred->id, STORAGE_CREDENTIAL_ID_LENGTH);

    /* publicKey */
    cbor_encode_uint(&encoder, CM_RESP_PUBLIC_KEY);
    uint8_t public_key[64];
    if (crypto_ecdsa_get_public_key(cred->private_key, public_key) == CRYPTO_OK) {
        cbor_encode_map_start(&encoder, 5);
        cbor_encode_int(&encoder, 1);
        cbor_encode_int(&encoder, 2);
        cbor_encode_int(&encoder, 3);
        cbor_encode_int(&encoder, cred->algorithm);
        cbor_encode_int(&encoder, -1);
        cbor_encode_int(&encoder, 1);
        cbor_encode_int(&encoder, -2);
        cbor_encode_bytes(&encoder, &public_key[0], 32);
        cbor_encode_int(&encoder, -3);
        cbor_encode_bytes(&encoder, &public_key[32], 32);
    }

    *response_len = cbor_encoder_get_size(&encoder);

    cm_state.current_cred_index++;

    if (cm_state.current_cred_index >= cm_state.total_creds) {
        cm_state.cred_enumeration_active = false;
    }

    return CTAP2_OK;
}

/**
 * @brief Delete a credential
 */
static uint8_t cm_delete_credential(const uint8_t *credential_id, uint8_t *response_data,
                                    size_t *response_len)
{
    if (storage_delete_credential(credential_id) != STORAGE_OK) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    *response_len = 0;
    LOG_INFO("Credential deleted successfully");

    return CTAP2_OK;
}

/**
 * @brief Main credential management command handler
 */
uint8_t ctap2_credential_management(const uint8_t *request_data, size_t request_len,
                                    uint8_t *response_data, size_t *response_len)
{
    LOG_INFO("Credential management command");

    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    uint64_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint8_t subcommand = 0;
    uint8_t pin_protocol = 0;
    uint8_t pin_auth[16];
    size_t pin_auth_len = 0;
    uint8_t rp_id_hash[32];
    uint8_t credential_id[STORAGE_CREDENTIAL_ID_LENGTH];
    bool has_subcommand = false;
    bool has_pin_auth = false;
    bool has_rp_id_hash = false;
    bool has_credential_id = false;

    /* Parse parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case CM_PARAM_SUBCOMMAND:
                if (cbor_decode_uint(&decoder, (uint64_t *) &subcommand) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_subcommand = true;
                break;

            case CM_PARAM_SUBCOMMAND_PARAMS: {
                /* Parse subcommand parameters */
                uint64_t params_map_size;
                if (cbor_decode_map_start(&decoder, &params_map_size) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }

                for (uint64_t j = 0; j < params_map_size; j++) {
                    uint64_t param_key;
                    if (cbor_decode_uint(&decoder, &param_key) != CBOR_OK) {
                        cbor_skip_value(&decoder);
                        cbor_skip_value(&decoder);
                        continue;
                    }

                    if (param_key == CM_RESP_RP_ID_HASH) {
                        if (cbor_decode_bytes(&decoder, rp_id_hash, 32) == CBOR_OK) {
                            has_rp_id_hash = true;
                        }
                    } else if (param_key == CM_RESP_CREDENTIAL_ID) {
                        uint64_t cred_map_size;
                        if (cbor_decode_map_start(&decoder, &cred_map_size) == CBOR_OK) {
                            for (uint64_t k = 0; k < cred_map_size; k++) {
                                char cred_key[8];
                                if (cbor_decode_text(&decoder, cred_key, sizeof(cred_key)) ==
                                        CBOR_OK &&
                                    strcmp(cred_key, "id") == 0) {
                                    if (cbor_decode_bytes(&decoder, credential_id,
                                                          STORAGE_CREDENTIAL_ID_LENGTH) ==
                                        CBOR_OK) {
                                        has_credential_id = true;
                                    }
                                } else {
                                    cbor_skip_value(&decoder);
                                }
                            }
                        }
                    } else {
                        cbor_skip_value(&decoder);
                    }
                }
                break;
            }

            case CM_PARAM_PIN_PROTOCOL:
                if (cbor_decode_uint(&decoder, (uint64_t *) &pin_protocol) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            case CM_PARAM_PIN_AUTH:
                pin_auth_len = sizeof(pin_auth);
                if (cbor_decode_bytes(&decoder, pin_auth, pin_auth_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_pin_auth = true;
                break;

            default:
                cbor_skip_value(&decoder);
                break;
        }
    }

    if (!has_subcommand) {
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    /* Verify PIN authentication */
    uint8_t pin_result = verify_cm_pin_auth(has_pin_auth ? pin_auth : NULL,
                                            has_pin_auth ? pin_auth_len : 0, pin_protocol);
    if (pin_result != CTAP2_OK) {
        return pin_result;
    }

    /* Execute subcommand */
    switch (subcommand) {
        case CM_GET_CREDS_METADATA:
            return cm_get_metadata(response_data, response_len);

        case CM_ENUMERATE_RPS_BEGIN:
            return cm_enumerate_rps_begin(response_data, response_len);

        case CM_ENUMERATE_RPS_GET_NEXT:
            return cm_enumerate_rps_get_next(response_data, response_len);

        case CM_ENUMERATE_CREDS_BEGIN:
            if (!has_rp_id_hash) {
                return CTAP2_ERR_MISSING_PARAMETER;
            }
            return cm_enumerate_creds_begin(rp_id_hash, response_data, response_len);

        case CM_ENUMERATE_CREDS_GET_NEXT:
            return cm_enumerate_creds_get_next(response_data, response_len);

        case CM_DELETE_CREDENTIAL:
            if (!has_credential_id) {
                return CTAP2_ERR_MISSING_PARAMETER;
            }
            return cm_delete_credential(credential_id, response_data, response_len);

        default:
            LOG_WARN("Unknown credential management subcommand: 0x%02X", subcommand);
            return CTAP2_ERR_INVALID_SUBCOMMAND;
    }
}
