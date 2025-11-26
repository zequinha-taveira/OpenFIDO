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

/* Client PIN Request Keys */
#define CP_KEY_PIN_PROTOCOL 0x01
#define CP_KEY_SUBCOMMAND 0x02
#define CP_KEY_KEY_AGREEMENT 0x03
#define CP_KEY_PIN_AUTH 0x04
#define CP_KEY_NEW_PIN_ENC 0x05
#define CP_KEY_PIN_HASH_ENC 0x06

/* Client PIN Subcommands */
#define CP_SUBCMD_GET_RETRIES 0x01
#define CP_SUBCMD_GET_KEY_AGREEMENT 0x02
#define CP_SUBCMD_SET_PIN 0x03
#define CP_SUBCMD_CHANGE_PIN 0x04
#define CP_SUBCMD_GET_PIN_TOKEN 0x05

/* Client PIN Response Keys */
#define CP_RESP_KEY_AGREEMENT 0x01
#define CP_RESP_PIN_TOKEN 0x02
#define CP_RESP_RETRIES 0x03

/* COSE Keys and Values */
#define COSE_KEY_KTY 1
#define COSE_KEY_ALG 3
#define COSE_KEY_CRV -1
#define COSE_KEY_X -2
#define COSE_KEY_Y -3

#define COSE_KTY_EC2 2
#define COSE_CRV_P256 1

/* AAGUID (Authenticator Attestation GUID) */
static const uint8_t AAGUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Global state */
static struct {
    bool initialized;
    uint8_t pending_assertions;
    storage_credential_t assertion_credentials[10];
} ctap2_state = {0};

/**
 * @brief Initialize the CTAP2 protocol handler.
 *
 * @return CTAP2_OK on success.
 */
uint8_t ctap2_init(void)
{
    LOG_INFO("Initializing CTAP2 protocol handler");

    ctap2_state.initialized = true;
    ctap2_state.pending_assertions = 0;

    return CTAP2_OK;
}

/* Forward declaration for the renamed function */
uint8_t ctap2_handle_client_pin(const uint8_t *request_data, size_t request_len,
                                uint8_t *response_data, size_t *response_len);

/**
 * @brief Helper function to detect weak PINs
 *
 * Checks for common weak patterns:
 * - All same digits (e.g., "1111")
 * - Sequential digits (e.g., "1234")
 *
 * @param pin PIN bytes to check
 * @param pin_len Length of PIN
 * @return true if PIN is weak, false otherwise
 */
static bool is_weak_pin(const uint8_t *pin, size_t pin_len)
{
    if (pin == NULL || pin_len < 4) {
        return true; /* Too short = weak */
    }

    /* Check for sequential digits (e.g., "1234") */
    bool all_sequential = true;
    for (size_t i = 1; i < pin_len; i++) {
        if (pin[i] != pin[i - 1] + 1) {
            all_sequential = false;
            break;
        }
    }
    if (all_sequential) {
        return true;
    }

    /* Check for repeated digits (e.g., "1111") */
    bool all_same = true;
    for (size_t i = 1; i < pin_len; i++) {
        if (pin[i] != pin[0]) {
            all_same = false;
            break;
        }
    }
    if (all_same) {
        return true;
    }

    return false;
}

/**
 * @brief Process a CTAP2 request.
 *
 * @param request Pointer to the request structure.
 * @param response Pointer to the response structure.
 * @return CTAP2 status code.
 */
uint8_t ctap2_process_request(const ctap2_request_t *request, ctap2_response_t *response)
{
    /* Input validation - Zero Trust: validate ALL inputs */
    if (!request || !response) {
        LOG_ERROR("NULL pointer in ctap2_process_request");
        return CTAP2_ERR_INVALID_PARAMETER;
    }

    if (!ctap2_state.initialized) {
        LOG_ERROR("CTAP2 not initialized");
        return CTAP2_ERR_INVALID_COMMAND;
    }

    /* Validate request data pointer if length > 0 */
    if (request->data_len > 0 && !request->data) {
        LOG_ERROR("Invalid request: data_len=%zu but data=NULL", request->data_len);
        return CTAP2_ERR_INVALID_PARAMETER;
    }

    /* Validate request length against maximum */
    if (request->data_len > CTAP2_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Request too large: %zu > %d", request->data_len, CTAP2_MAX_MESSAGE_SIZE);
        return CTAP2_ERR_REQUEST_TOO_LARGE;
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
            return ctap2_handle_client_pin(request->data, request->data_len, response->data,
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

/**
 * @brief Handle the authenticatorGetInfo command.
 *
 * @param response_data Buffer to store the response.
 * @param response_len Pointer to store the response length.
 * @return CTAP2 status code.
 */
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

/**
 * @brief Handle the clientPIN command.
 *
 * @param request_data Pointer to the request data.
 * @param request_len Length of the request data.
 * @param response_data Buffer to store the response.
 * @param response_len Pointer to store the response length.
 * @return CTAP2 status code.
 */
uint8_t ctap2_handle_client_pin(const uint8_t *request_data, size_t request_len,
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
            case CP_KEY_PIN_PROTOCOL:
                cbor_skip_value(&decoder);
                break;
            case CP_KEY_SUBCOMMAND:
                if (cbor_decode_uint(&decoder, &sub_command) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_sub_command = true;
                break;
            case CP_KEY_NEW_PIN_ENC: {
                size_t actual_len = sizeof(new_pin_enc);

                /* Decode and capture actual length */
                int decode_result = cbor_decode_bytes(&decoder, new_pin_enc, &actual_len);

                if (decode_result != CBOR_OK) {
                    crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                    return CTAP2_ERR_INVALID_CBOR;
                }

                /* Validate actual decoded length */
                if (actual_len == 0) {
                    crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                    LOG_WARN("Empty PIN encrypted data received");
                    return CTAP2_ERR_PIN_POLICY_VIOLATION;
                }

                if (actual_len > sizeof(new_pin_enc)) {
                    crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                    LOG_WARN("PIN encrypted data exceeds maximum size: %zu > %zu", actual_len,
                             sizeof(new_pin_enc));
                    return CTAP2_ERR_INVALID_LENGTH;
                }

                new_pin_enc_len = actual_len;
                break;
            }
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
        case CP_SUBCMD_GET_RETRIES:
            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, CP_RESP_RETRIES);
            cbor_encode_uint(&encoder, storage_get_pin_retries());
            *response_len = cbor_encoder_get_size(&encoder);
            LOG_INFO("ClientPIN: GetRetries");
            return CTAP2_OK;

        case CP_SUBCMD_GET_KEY_AGREEMENT: {
            /* Generate ephemeral ECDH key pair */
            uint8_t private_key[32];
            uint8_t public_key[64];

            if (crypto_ecdsa_generate_keypair(private_key, public_key) != CRYPTO_OK) {
                crypto_secure_zero(private_key, sizeof(private_key));
                return CTAP2_ERR_PROCESSING;
            }

            cbor_encode_map_start(&encoder, 1);
            cbor_encode_uint(&encoder, CP_RESP_KEY_AGREEMENT);

            /* Encode COSE_Key */
            cbor_encode_map_start(&encoder, 5);
            cbor_encode_int(&encoder, COSE_KEY_KTY);
            cbor_encode_int(&encoder, COSE_KTY_EC2);
            cbor_encode_int(&encoder, COSE_KEY_ALG);
            cbor_encode_int(&encoder, COSE_ALG_ES256);
            cbor_encode_int(&encoder, COSE_KEY_CRV);
            cbor_encode_int(&encoder, COSE_CRV_P256);
            cbor_encode_int(&encoder, COSE_KEY_X);
            cbor_encode_bytes(&encoder, &public_key[0], 32);
            cbor_encode_int(&encoder, COSE_KEY_Y);
            cbor_encode_bytes(&encoder, &public_key[32], 32);

            *response_len = cbor_encoder_get_size(&encoder);

            /* CRITICAL: Securely wipe private key before returning */
            crypto_secure_zero(private_key, sizeof(private_key));

            LOG_INFO("ClientPIN: GetKeyAgreement");
            return CTAP2_OK;
        }

        case CP_SUBCMD_SET_PIN: {
            /* Verify PIN is not already set */
            if (storage_is_pin_set()) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                return CTAP2_ERR_PIN_INVALID;
            }

            /* Validate encrypted PIN was provided */
            if (new_pin_enc_len == 0 || new_pin_enc_len > 64) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                return CTAP2_ERR_MISSING_PARAMETER;
            }

            /* TODO: In production implementation:
             * 1. Retrieve platform's public key from CP_KEY_KEY_AGREEMENT
             * 2. Compute ECDH shared secret with our private key
             * 3. Derive encryption key using HKDF-SHA256
             * 4. Decrypt newPinEnc using AES-256-CBC
             * For this implementation, we will simulate decryption
             */

            /* Simulate decryption (in production, use crypto_aes_gcm_decrypt) */
            uint8_t decrypted_pin[STORAGE_PIN_MAX_LENGTH];
            size_t decrypted_pin_len = new_pin_enc_len;
            memcpy(decrypted_pin, new_pin_enc, new_pin_enc_len);

            /* Validate PIN length AFTER decryption */
            if (decrypted_pin_len < STORAGE_PIN_MIN_LENGTH ||
                decrypted_pin_len > STORAGE_PIN_MAX_LENGTH) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                crypto_secure_zero(decrypted_pin, sizeof(decrypted_pin));
                return CTAP2_ERR_PIN_POLICY_VIOLATION;
            }

            /* Check for weak PINs */
            if (is_weak_pin(decrypted_pin, decrypted_pin_len)) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                crypto_secure_zero(decrypted_pin, sizeof(decrypted_pin));
                LOG_WARN("Weak PIN rejected");
                return CTAP2_ERR_PIN_POLICY_VIOLATION;
            }

            /* Set PIN (storage_set_pin should hash with SHA-256 before storing) */
            int result = storage_set_pin(decrypted_pin, decrypted_pin_len);

            /* CRITICAL: Securely wipe sensitive data */
            crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
            crypto_secure_zero(decrypted_pin, sizeof(decrypted_pin));

            if (result != STORAGE_OK) {
                return CTAP2_ERR_PROCESSING;
            }

            *response_len = 0;
            LOG_INFO("ClientPIN: SetPIN");
            return CTAP2_OK;
        }

        case CP_SUBCMD_CHANGE_PIN: {
            /* Change existing PIN */
            if (!storage_is_pin_set()) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                return CTAP2_ERR_PIN_NOT_SET;
            }

            /* Validate encrypted PIN was provided */
            if (new_pin_enc_len == 0 || new_pin_enc_len > 64) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                return CTAP2_ERR_MISSING_PARAMETER;
            }

            /* In production: */
            /* 1. Decrypt pinHashEnc using shared secret */
            /* 2. Verify current PIN */
            /* 3. Decrypt newPinEnc */
            /* 4. Set new PIN */

            /* Simulate decryption */
            uint8_t decrypted_pin[STORAGE_PIN_MAX_LENGTH];
            size_t decrypted_pin_len = new_pin_enc_len;
            memcpy(decrypted_pin, new_pin_enc, new_pin_enc_len);

            /* Validate and check for weak PINs */
            if (decrypted_pin_len < STORAGE_PIN_MIN_LENGTH ||
                decrypted_pin_len > STORAGE_PIN_MAX_LENGTH ||
                is_weak_pin(decrypted_pin, decrypted_pin_len)) {
                crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
                crypto_secure_zero(decrypted_pin, sizeof(decrypted_pin));
                return CTAP2_ERR_PIN_POLICY_VIOLATION;
            }

            int result = storage_set_pin(decrypted_pin, decrypted_pin_len);

            /* Cleanup sensitive data */
            crypto_secure_zero(new_pin_enc, sizeof(new_pin_enc));
            crypto_secure_zero(decrypted_pin, sizeof(decrypted_pin));

            if (result != STORAGE_OK) {
                return CTAP2_ERR_PROCESSING;
            }

            *response_len = 0;
            LOG_INFO("ClientPIN: ChangePIN");
            return CTAP2_OK;
        }

        case CP_SUBCMD_GET_PIN_TOKEN: {
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
            cbor_encode_uint(&encoder, CP_RESP_PIN_TOKEN);
            cbor_encode_bytes(&encoder, pin_token, sizeof(pin_token));

            *response_len = cbor_encoder_get_size(&encoder);

            /* CRITICAL: Securely wipe PIN token after encoding */
            crypto_secure_zero(pin_token, sizeof(pin_token));

            LOG_INFO("ClientPIN: GetPINToken");
            return CTAP2_OK;
        }

        default:
            LOG_WARN("ClientPIN: Unknown subcommand %llu", sub_command);
            return CTAP2_ERR_INVALID_SUBCOMMAND;
    }
}

/**
 * @brief Reset the authenticator.
 *
 * @return CTAP2 status code.
 */
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

/**
 * @brief Get the next assertion from the pending list.
 *
 * @param response_data Buffer to store the response.
 * @param response_len Pointer to store the response length.
 * @return CTAP2 status code.
 */
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
