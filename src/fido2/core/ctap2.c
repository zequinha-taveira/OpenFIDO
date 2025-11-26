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
    }

    return CTAP2_ERR_INVALID_COMMAND;
}

uint8_t ctap2_get_info(uint8_t * response_data, size_t * response_len)
{
    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    /* Start response map */
    cbor_encode_map_start(&encoder, 8);

    /* 0x01: versions */
    cbor_encode_uint(&encoder, GETINFO_VERSIONS);
    cbor_encode_array_start(&encoder, 3);
    cbor_encode_text(&encoder, "FIDO_2_1", 8);
    cbor_encode_text(&encoder, "FIDO_2_0", 8);
    cbor_encode_text(&encoder, "U2F_V2", 6);

    /* 0x02: extensions */
    cbor_encode_uint(&encoder, GETINFO_EXTENSIONS);
    cbor_encode_array_start(&encoder, 2);
    cbor_encode_text(&encoder, "hmac-secret", 11);
    cbor_encode_text(&encoder, "credProtect", 11);

    /* 0x03: AAGUID */
    cbor_encode_uint(&encoder, GETINFO_AAGUID);
    static const uint8_t aaguid[16] = {0};
    cbor_encode_bytes(&encoder, aaguid, 16);

    /* 0x04: options */
    cbor_encode_uint(&encoder, GETINFO_OPTIONS);
    cbor_encode_map_start(&encoder, 5);
    cbor_encode_text(&encoder, "rk", 2);
    cbor_encode_bool(&encoder, true);
    cbor_encode_text(&encoder, "up", 2);
    cbor_encode_bool(&encoder, true);
    cbor_encode_text(&encoder, "uv", 2);
    cbor_encode_bool(&encoder, storage_is_pin_set());
    cbor_encode_text(&encoder, "plat", 4);
    cbor_encode_bool(&encoder, false);
    cbor_encode_text(&encoder, "clientPin", 9);
    cbor_encode_bool(&encoder, storage_is_pin_set());

    /* 0x05: maxMsgSize */
    cbor_encode_uint(&encoder, GETINFO_MAX_MSG_SIZE);
    cbor_encode_uint(&encoder, CTAP2_MAX_MESSAGE_SIZE);

    /* 0x06: pinProtocols */
    cbor_encode_uint(&encoder, GETINFO_PIN_PROTOCOLS);
    cbor_encode_array_start(&encoder, 1);
    cbor_encode_uint(&encoder, 1); /* PIN protocol v1 */

    /* 0x07: maxCredentialCountInList */
    cbor_encode_uint(&encoder, GETINFO_MAX_CREDS_IN_LIST);
    cbor_encode_uint(&encoder, 10);

    /* 0x08: maxCredentialIdLength */
    cbor_encode_uint(&encoder, GETINFO_MAX_CRED_ID_LENGTH);
    cbor_encode_uint(&encoder, STORAGE_CREDENTIAL_ID_LENGTH);

    *response_len = cbor_encoder_get_size(&encoder);
    LOG_INFO("GetInfo completed");
    return CTAP2_OK;
}

uint8_t ctap2_client_pin(const uint8_t *request_data, size_t request_len,
                         uint8_t *response_data, size_t *response_len)
{
    LOG_INFO("ClientPIN command");

    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    uint64_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    uint64_t sub_command = 0;
    uint8_t new_pin_enc[64];
    size_t new_pin_enc_len = 0;
    bool has_sub_command = false;

    /* Parse parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case 0x01: /* pinProtocol */
                cbor_skip_value(&decoder);
                break;
            case 0x02: /* subCommand */
                if (cbor_decode_uint(&decoder, &sub_command) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_sub_command = true;
                break;
            case 0x05: /* newPinEnc */
                new_pin_enc_len = sizeof(new_pin_enc);
                if (cbor_decode_bytes(&decoder, new_pin_enc, new_pin_enc_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;
            default:
                cbor_skip_value(&decoder);
                break;
        }
    }

    if (!has_sub_command) {
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    switch (sub_command) {
        case 0x01: /* getRetries */
            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, 0x03); /* retries */
            cbor_encode_uint(&encoder, storage_get_pin_retries());
            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetRetries");
            return CTAP2_OK;

        case 0x02: { /* getKeyAgreement */
            /* Generate ephemeral ECDH key pair */
            uint8_t private_key[32];
            uint8_t public_key[64];
            crypto_ecdsa_generate_keypair(private_key, public_key);

            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, 0x01); /* keyAgreement */

            /* Encode COSE_Key */
            cbor_encode_map_start(&encoder, 5);
            cbor_encode_int(&encoder, 1); /* kty */
            cbor_encode_int(&encoder, 2); /* EC2 */
            cbor_encode_int(&encoder, 3); /* alg */
            cbor_encode_int(&encoder, COSE_ALG_ES256);
            cbor_encode_int(&encoder, -1); /* crv */
            cbor_encode_int(&encoder, 1);  /* P-256 */
            cbor_encode_int(&encoder, -2); /* x */
            cbor_encode_bytes(&encoder, &public_key[0], 32);
            cbor_encode_int(&encoder, -3); /* y */
            cbor_encode_bytes(&encoder, &public_key[32], 32);

            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetKeyAgreement");
            return CTAP2_OK;
        }

        case 0x03: { /* setPIN */
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

        case 0x04: { /* changePIN */
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

        case 0x05: { /* getPINToken */
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

uint8_t ctap2_get_next_assertion(uint8_t * response_data, size_t * response_len)
{
    if (ctap2_state.pending_assertions == 0) {
        return CTAP2_ERR_NO_OPERATION_PENDING;
    }

    /* Return next assertion from pending list */
    ctap2_state.pending_assertions--;

    LOG_INFO("GetNextAssertion: %d remaining", ctap2_state.pending_assertions);
    return CTAP2_OK;
}
