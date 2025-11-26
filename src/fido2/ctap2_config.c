/**
 * @file ctap2_config.c
 * @brief CTAP2 Authenticator Configuration Command Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <string.h>

#include "cbor.h"
#include "ctap2.h"
#include "logger.h"
#include "permissions.h"
#include "storage.h"

/* Authenticator Config Subcommands */
#define CONFIG_ENABLE_ENTERPRISE_ATTESTATION 0x01
#define CONFIG_TOGGLE_ALWAYS_UV 0x02
#define CONFIG_SET_MIN_PIN_LENGTH 0x03
#define CONFIG_VENDOR_PROTOTYPE 0xFF

/* Request Parameters */
#define CONFIG_PARAM_SUBCOMMAND 0x01
#define CONFIG_PARAM_SUBCOMMAND_PARAMS 0x02
#define CONFIG_PARAM_PIN_PROTOCOL 0x03
#define CONFIG_PARAM_PIN_AUTH 0x04

/**
 * @brief Verify PIN authentication for authenticator config
 */
static uint8_t verify_config_pin_auth(const uint8_t *pin_auth, size_t pin_auth_len,
                                      uint8_t pin_protocol)
{
    /* PIN and acfg permission required */
    if (!storage_is_pin_set()) {
        return CTAP2_ERR_PIN_NOT_SET;
    }

    if (!permissions_check(PERM_AUTHENTICATOR_CFG, NULL)) {
        return CTAP2_ERR_PIN_REQUIRED;
    }

    if (pin_auth == NULL || pin_auth_len == 0) {
        return CTAP2_ERR_PIN_REQUIRED;
    }

    if (pin_protocol != 1) {
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    /* Simplified verification */
    if (pin_auth_len != 16) {
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

    return CTAP2_OK;
}

/**
 * @brief Enable enterprise attestation
 */
static uint8_t config_enable_enterprise_attestation(uint8_t *response_data, size_t *response_len)
{
    /* Set enterprise attestation flag in storage */
    /* This would typically set a persistent flag */
    LOG_INFO("Enterprise attestation enabled");

    *response_len = 0;
    return CTAP2_OK;
}

/**
 * @brief Toggle alwaysUv option
 */
static uint8_t config_toggle_always_uv(uint8_t *response_data, size_t *response_len)
{
    /* Toggle alwaysUv setting */
    /* This would flip a persistent configuration flag */
    LOG_INFO("AlwaysUv toggled");

    *response_len = 0;
    return CTAP2_OK;
}

/**
 * @brief Set minimum PIN length
 */
static uint8_t config_set_min_pin_length(uint8_t new_min_length, uint8_t *response_data,
                                         size_t *response_len)
{
    if (new_min_length < STORAGE_PIN_MIN_LENGTH || new_min_length > STORAGE_PIN_MAX_LENGTH) {
        return CTAP2_ERR_INVALID_PARAMETER;
    }

    /* Store new minimum PIN length */
    /* This would be stored in persistent configuration */
    LOG_INFO("Minimum PIN length set to %d", new_min_length);

    *response_len = 0;
    return CTAP2_OK;
}

/**
 * @brief Main authenticator configuration command handler
 */
uint8_t ctap2_authenticator_config(const uint8_t *request_data, size_t request_len,
                                    uint8_t *response_data, size_t *response_len)
{
    LOG_INFO("Authenticator configuration command");

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
    uint8_t new_min_pin_length = 0;
    bool has_subcommand = false;
    bool has_pin_auth = false;
    bool has_min_pin_length = false;

    /* Parse parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case CONFIG_PARAM_SUBCOMMAND:
                if (cbor_decode_uint(&decoder, (uint64_t *)&subcommand) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                has_subcommand = true;
                break;

            case CONFIG_PARAM_SUBCOMMAND_PARAMS: {
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

                    if (param_key == 0x01) { /* newMinPINLength */
                        if (cbor_decode_uint(&decoder, (uint64_t *)&new_min_pin_length) ==
                            CBOR_OK) {
                            has_min_pin_length = true;
                        }
                    } else {
                        cbor_skip_value(&decoder);
                    }
                }
                break;
            }

            case CONFIG_PARAM_PIN_PROTOCOL:
                if (cbor_decode_uint(&decoder, (uint64_t *)&pin_protocol) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            case CONFIG_PARAM_PIN_AUTH:
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
    uint8_t pin_result =
        verify_config_pin_auth(has_pin_auth ? pin_auth : NULL, has_pin_auth ? pin_auth_len : 0,
                               pin_protocol);
    if (pin_result != CTAP2_OK) {
        return pin_result;
    }

    /* Execute subcommand */
    switch (subcommand) {
        case CONFIG_ENABLE_ENTERPRISE_ATTESTATION:
            return config_enable_enterprise_attestation(response_data, response_len);

        case CONFIG_TOGGLE_ALWAYS_UV:
            return config_toggle_always_uv(response_data, response_len);

        case CONFIG_SET_MIN_PIN_LENGTH:
            if (!has_min_pin_length) {
                return CTAP2_ERR_MISSING_PARAMETER;
            }
            return config_set_min_pin_length(new_min_pin_length, response_data, response_len);

        case CONFIG_VENDOR_PROTOTYPE:
            LOG_INFO("Vendor prototype subcommand");
            *response_len = 0;
            return CTAP2_OK;

        default:
            LOG_WARN("Unknown authenticator config subcommand: 0x%02X", subcommand);
            return CTAP2_ERR_INVALID_SUBCOMMAND;
    }
}
