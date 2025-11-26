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
    cbor_encode_array_start(&encoder, 3);
    cbor_encode_text(&encoder, "FIDO_2_1", 8);
    cbor_encode_text(&encoder, "FIDO_2_0", 8);
    cbor_encode_text(&encoder, "U2F_V2", 6);

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
