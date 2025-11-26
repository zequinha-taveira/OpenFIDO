/**
 * @file openpgp.c
 * @brief OpenPGP Card Core Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "openpgp.h"

#include <string.h>

#include "../crypto/crypto.h"
#include "../storage/storage.h"
#include "../usb/usb_ccid.h"
#include "../utils/logger.h"

/* Forward declarations from usb_ccid.h */
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
    uint8_t data[255];
    uint8_t le;
} apdu_command_t;

typedef struct {
    uint8_t data[256];
    uint16_t len;
    uint8_t sw1;
    uint8_t sw2;
} apdu_response_t;

/* OpenPGP State */
static openpgp_state_t openpgp_state;
static openpgp_key_slot_t key_slots[3];

/* Default PINs */
static const uint8_t default_pin_user[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36}; /* "123456" */
static const uint8_t default_pin_admin[] = {0x31, 0x32, 0x33, 0x34,
                                            0x35, 0x36, 0x37, 0x38}; /* "12345678" */

/**
 * @brief Build status word response
 */
static void set_response_sw(apdu_response_t *resp, uint16_t sw)
{
    resp->sw1 = (sw >> 8) & 0xFF;
    resp->sw2 = sw & 0xFF;
}

/**
 * @brief Handle VERIFY command
 */
static int handle_verify(const apdu_command_t *cmd, apdu_response_t *resp)
{
    uint8_t pin_ref = cmd->p2;

    /* Check if verifying status */
    if (cmd->lc == 0) {
        bool verified = false;
        if (pin_ref == OPENPGP_PIN_USER) {
            verified = openpgp_state.pin_user_verified;
        } else if (pin_ref == OPENPGP_PIN_ADMIN) {
            verified = openpgp_state.pin_admin_verified;
        }

        if (verified) {
            set_response_sw(resp, OPENPGP_SW_SUCCESS);
        } else {
            set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        }
        resp->len = 0;
        return 0;
    }

    /* Verify PIN */
    int ret = openpgp_verify_pin(pin_ref, cmd->data, cmd->lc);

    if (ret == 0) {
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else if (ret == -2) {
        set_response_sw(resp, OPENPGP_SW_AUTH_BLOCKED);
    } else {
        uint8_t retries = (pin_ref == OPENPGP_PIN_USER) ? openpgp_state.pin_user_retries
                                                        : openpgp_state.pin_admin_retries;
        set_response_sw(resp, OPENPGP_SW_VERIFY_FAIL | retries);
    }

    resp->len = 0;
    return 0;
}

/**
 * @brief Handle CHANGE REFERENCE DATA command
 */
static int handle_change_reference(const apdu_command_t *cmd, apdu_response_t *resp)
{
    uint8_t pin_ref = cmd->p2;

    /* Parse old and new PIN */
    if (cmd->lc < 12) {
        set_response_sw(resp, OPENPGP_SW_WRONG_LENGTH);
        resp->len = 0;
        return 0;
    }

    /* Assuming format: old_pin || new_pin */
    size_t half = cmd->lc / 2;
    const uint8_t *old_pin = cmd->data;
    const uint8_t *new_pin = &cmd->data[half];

    int ret = openpgp_change_pin(pin_ref, old_pin, half, new_pin, half);

    if (ret == 0) {
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
    }

    resp->len = 0;
    return 0;
}

/**
 * @brief Handle GET DATA command
 */
static int handle_get_data(const apdu_command_t *cmd, apdu_response_t *resp)
{
    uint16_t tag = (cmd->p1 << 8) | cmd->p2;

    LOG_DEBUG("OpenPGP: GET DATA tag=0x%04X", tag);

    size_t data_len = 0;
    int ret = openpgp_get_data(tag, resp->data, &data_len);

    if (ret == 0) {
        resp->len = data_len;
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else {
        resp->len = 0;
        set_response_sw(resp, OPENPGP_SW_FILE_NOT_FOUND);
    }

    return 0;
}

/**
 * @brief Handle PUT DATA command
 */
static int handle_put_data(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Must be authenticated with admin PIN */
    if (!openpgp_state.pin_admin_verified) {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    uint16_t tag = (cmd->p1 << 8) | cmd->p2;

    LOG_DEBUG("OpenPGP: PUT DATA tag=0x%04X len=%d", tag, cmd->lc);

    int ret = openpgp_put_data(tag, cmd->data, cmd->lc);

    if (ret == 0) {
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else {
        set_response_sw(resp, OPENPGP_SW_INCORRECT_PARAM);
    }

    resp->len = 0;
    return 0;
}

/**
 * @brief Handle GENERATE ASYMMETRIC KEY PAIR command
 */
static int handle_generate_asymmetric(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Must be authenticated with admin PIN */
    if (!openpgp_state.pin_admin_verified) {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    uint8_t key_ref = cmd->p2;

    LOG_INFO("OpenPGP: Generate key pair for slot 0x%02X", key_ref);

    /* Default to ECC P-256 */
    uint8_t algorithm = OPENPGP_ALGO_ECDSA;
    uint16_t key_size = 256;

    size_t public_key_len = 0;
    int ret = openpgp_generate_key(key_ref, algorithm, key_size, resp->data, &public_key_len);

    if (ret == 0) {
        resp->len = public_key_len;
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else {
        resp->len = 0;
        set_response_sw(resp, OPENPGP_SW_INCORRECT_PARAM);
    }

    return 0;
}

/**
 * @brief Handle PSO (Perform Security Operation) command
 */
static int handle_pso(const apdu_command_t *cmd, apdu_response_t *resp)
{
    uint16_t operation = (cmd->p1 << 8) | cmd->p2;

    LOG_DEBUG("OpenPGP: PSO operation=0x%04X", operation);

    if (operation == OPENPGP_PSO_CDS) {
        /* Compute Digital Signature */
        if (!openpgp_state.pin_user_verified) {
            set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
            resp->len = 0;
            return 0;
        }

        size_t sig_len = 0;
        int ret = openpgp_sign(cmd->data, cmd->lc, resp->data, &sig_len);

        if (ret == 0) {
            resp->len = sig_len;
            set_response_sw(resp, OPENPGP_SW_SUCCESS);
            openpgp_state.sig_counter++;
        } else {
            resp->len = 0;
            set_response_sw(resp, OPENPGP_SW_INCORRECT_PARAM);
        }

    } else if (operation == OPENPGP_PSO_DEC) {
        /* Decipher */
        if (!openpgp_state.pin_user_verified) {
            set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
            resp->len = 0;
            return 0;
        }

        size_t plain_len = 0;
        int ret = openpgp_decipher(cmd->data, cmd->lc, resp->data, &plain_len);

        if (ret == 0) {
            resp->len = plain_len;
            set_response_sw(resp, OPENPGP_SW_SUCCESS);
        } else {
            resp->len = 0;
            set_response_sw(resp, OPENPGP_SW_INCORRECT_PARAM);
        }

    } else {
        /* Unsupported operation */
        set_response_sw(resp, OPENPGP_SW_INCORRECT_P1P2);
        resp->len = 0;
    }

    return 0;
}

/**
 * @brief Handle INTERNAL AUTHENTICATE command
 */
static int handle_internal_authenticate(const apdu_command_t *cmd, apdu_response_t *resp)
{
    if (!openpgp_state.pin_user_verified) {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    size_t resp_len = 0;
    int ret = openpgp_authenticate(cmd->data, cmd->lc, resp->data, &resp_len);

    if (ret == 0) {
        resp->len = resp_len;
        set_response_sw(resp, OPENPGP_SW_SUCCESS);
    } else {
        resp->len = 0;
        set_response_sw(resp, OPENPGP_SW_INCORRECT_PARAM);
    }

    return 0;
}

/**
 * @brief Handle TERMINATE DF command
 */
static int handle_terminate(const apdu_command_t *cmd, apdu_response_t *resp)
{
    if (!openpgp_state.pin_admin_verified) {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    openpgp_state.terminated = true;
    set_response_sw(resp, OPENPGP_SW_SUCCESS);
    resp->len = 0;

    LOG_INFO("OpenPGP: Application terminated");
    return 0;
}

/**
 * @brief Handle ACTIVATE FILE command
 */
static int handle_activate(const apdu_command_t *cmd, apdu_response_t *resp)
{
    if (!openpgp_state.pin_admin_verified) {
        set_response_sw(resp, OPENPGP_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    openpgp_state.terminated = false;
    set_response_sw(resp, OPENPGP_SW_SUCCESS);
    resp->len = 0;

    LOG_INFO("OpenPGP: Application activated");
    return 0;
}

int openpgp_init(void)
{
    LOG_INFO("Initializing OpenPGP module");

    memset(&openpgp_state, 0, sizeof(openpgp_state));
    memset(key_slots, 0, sizeof(key_slots));

    /* Set default PINs */
    memcpy(openpgp_state.pin_user, default_pin_user, sizeof(default_pin_user));
    openpgp_state.pin_user_len = sizeof(default_pin_user);
    memcpy(openpgp_state.pin_admin, default_pin_admin, sizeof(default_pin_admin));
    openpgp_state.pin_admin_len = sizeof(default_pin_admin);

    openpgp_state.pin_user_retries = OPENPGP_PIN_MAX_RETRIES;
    openpgp_state.pin_admin_retries = OPENPGP_PIN_MAX_RETRIES;

    /* Initialize key slots */
    key_slots[0].key_ref = OPENPGP_KEY_SIG;
    key_slots[1].key_ref = OPENPGP_KEY_DEC;
    key_slots[2].key_ref = OPENPGP_KEY_AUT;

    /* Set default cardholder data */
    strcpy(openpgp_state.name, "OpenFIDO User");
    strcpy(openpgp_state.language, "en");
    openpgp_state.sex = '9'; /* Not announced */

    /* Register with CCID */
    usb_ccid_register_app((const uint8_t *) OPENPGP_AID, OPENPGP_AID_LEN, openpgp_handle_apdu);

    LOG_INFO("OpenPGP initialized successfully");
    return 0;
}

int openpgp_handle_apdu(const void *cmd_ptr, void *resp_ptr)
{
    const apdu_command_t *cmd = (const apdu_command_t *) cmd_ptr;
    apdu_response_t *resp = (apdu_response_t *) resp_ptr;

    /* Check if terminated */
    if (openpgp_state.terminated && cmd->ins != OPENPGP_INS_ACTIVATE) {
        set_response_sw(resp, OPENPGP_SW_CONDITIONS_NOT_SAT);
        resp->len = 0;
        return 0;
    }

    LOG_DEBUG("OpenPGP: APDU INS=0x%02X P1=0x%02X P2=0x%02X Lc=%d", cmd->ins, cmd->p1, cmd->p2,
              cmd->lc);

    switch (cmd->ins) {
        case OPENPGP_INS_VERIFY:
            return handle_verify(cmd, resp);

        case OPENPGP_INS_CHANGE_REFERENCE:
            return handle_change_reference(cmd, resp);

        case OPENPGP_INS_GET_DATA:
            return handle_get_data(cmd, resp);

        case OPENPGP_INS_PUT_DATA:
            return handle_put_data(cmd, resp);

        case OPENPGP_INS_GENERATE_ASYMMETRIC:
            return handle_generate_asymmetric(cmd, resp);

        case OPENPGP_INS_PSO:
            return handle_pso(cmd, resp);

        case OPENPGP_INS_INTERNAL_AUTH:
            return handle_internal_authenticate(cmd, resp);

        case OPENPGP_INS_TERMINATE:
            return handle_terminate(cmd, resp);

        case OPENPGP_INS_ACTIVATE:
            return handle_activate(cmd, resp);

        default:
            LOG_WARN("OpenPGP: Unsupported instruction 0x%02X", cmd->ins);
            set_response_sw(resp, OPENPGP_SW_INS_NOT_SUPPORTED);
            resp->len = 0;
            return 0;
    }
}

int openpgp_verify_pin(uint8_t pin_ref, const uint8_t *pin, size_t pin_len)
{
    if (pin_ref == OPENPGP_PIN_USER) {
        if (openpgp_state.pin_user_retries == 0) {
            return -2; /* Blocked */
        }

        if (pin_len == openpgp_state.pin_user_len &&
            memcmp(pin, openpgp_state.pin_user, pin_len) == 0) {
            openpgp_state.pin_user_verified = true;
            openpgp_state.pin_user_retries = OPENPGP_PIN_MAX_RETRIES;
            LOG_INFO("OpenPGP: User PIN verified");
            return 0;
        }

        openpgp_state.pin_user_verified = false;
        openpgp_state.pin_user_retries--;
        LOG_WARN("OpenPGP: User PIN verification failed, %d retries remaining",
                 openpgp_state.pin_user_retries);
        return -1;

    } else if (pin_ref == OPENPGP_PIN_ADMIN) {
        if (openpgp_state.pin_admin_retries == 0) {
            return -2; /* Blocked */
        }

        if (pin_len == openpgp_state.pin_admin_len &&
            memcmp(pin, openpgp_state.pin_admin, pin_len) == 0) {
            openpgp_state.pin_admin_verified = true;
            openpgp_state.pin_admin_retries = OPENPGP_PIN_MAX_RETRIES;
            LOG_INFO("OpenPGP: Admin PIN verified");
            return 0;
        }

        openpgp_state.pin_admin_verified = false;
        openpgp_state.pin_admin_retries--;
        LOG_WARN("OpenPGP: Admin PIN verification failed, %d retries remaining",
                 openpgp_state.pin_admin_retries);
        return -1;
    }

    return -1;
}

int openpgp_change_pin(uint8_t pin_ref, const uint8_t *old_pin, size_t old_len,
                       const uint8_t *new_pin, size_t new_len)
{
    if (new_len < OPENPGP_PIN_MIN_LENGTH || new_len > OPENPGP_PIN_MAX_LENGTH) {
        return -1;
    }

    if (pin_ref == OPENPGP_PIN_USER) {
        if (old_len != openpgp_state.pin_user_len ||
            memcmp(old_pin, openpgp_state.pin_user, old_len) != 0) {
            return -1;
        }

        memcpy(openpgp_state.pin_user, new_pin, new_len);
        openpgp_state.pin_user_len = new_len;
        LOG_INFO("OpenPGP: User PIN changed");
        return 0;

    } else if (pin_ref == OPENPGP_PIN_ADMIN) {
        if (old_len != openpgp_state.pin_admin_len ||
            memcmp(old_pin, openpgp_state.pin_admin, old_len) != 0) {
            return -1;
        }

        memcpy(openpgp_state.pin_admin, new_pin, new_len);
        openpgp_state.pin_admin_len = new_len;
        LOG_INFO("OpenPGP: Admin PIN changed");
        return 0;
    }

    return -1;
}

int openpgp_generate_key(uint8_t key_ref, uint8_t algorithm, uint16_t key_size, uint8_t *public_key,
                         size_t *public_key_len)
{
    /* Find key slot */
    openpgp_key_slot_t *slot = NULL;
    for (int i = 0; i < 3; i++) {
        if (key_slots[i].key_ref == key_ref) {
            slot = &key_slots[i];
            break;
        }
    }

    if (slot == NULL) {
        return -1;
    }

    /* Generate key based on algorithm */
    /* This is a placeholder - actual implementation would use crypto module */
    slot->algorithm = algorithm;
    slot->key_size = key_size;
    slot->generated = true;

    /* Return dummy public key for now */
    *public_key_len = 0;

    LOG_INFO("OpenPGP: Generated key for slot 0x%02X", key_ref);
    return 0;
}

int openpgp_sign(const uint8_t *data, size_t data_len, uint8_t *signature, size_t *sig_len)
{
    /* TODO: Implement signature generation */
    *sig_len = 0;
    return -1;
}

int openpgp_decipher(const uint8_t *data, size_t data_len, uint8_t *plaintext, size_t *plain_len)
{
    /* TODO: Implement decryption */
    *plain_len = 0;
    return -1;
}

int openpgp_authenticate(const uint8_t *challenge, size_t challenge_len, uint8_t *response,
                         size_t *resp_len)
{
    /* TODO: Implement authentication */
    *resp_len = 0;
    return -1;
}

int openpgp_get_data(uint16_t tag, uint8_t *data, size_t *data_len)
{
    switch (tag) {
        case OPENPGP_DO_NAME:
            *data_len = strlen(openpgp_state.name);
            memcpy(data, openpgp_state.name, *data_len);
            return 0;

        case OPENPGP_DO_LANG:
            *data_len = strlen(openpgp_state.language);
            memcpy(data, openpgp_state.language, *data_len);
            return 0;

        case OPENPGP_DO_SEX:
            data[0] = openpgp_state.sex;
            *data_len = 1;
            return 0;

        case OPENPGP_DO_URL:
            *data_len = strlen(openpgp_state.url);
            memcpy(data, openpgp_state.url, *data_len);
            return 0;

        default:
            *data_len = 0;
            return -1;
    }
}

int openpgp_put_data(uint16_t tag, const uint8_t *data, size_t data_len)
{
    switch (tag) {
        case OPENPGP_DO_NAME:
            if (data_len > OPENPGP_NAME_MAX_LENGTH) {
                return -1;
            }
            memcpy(openpgp_state.name, data, data_len);
            openpgp_state.name[data_len] = '\0';
            LOG_INFO("OpenPGP: Set cardholder name");
            return 0;

        case OPENPGP_DO_LANG:
            if (data_len > OPENPGP_LANG_MAX_LENGTH) {
                return -1;
            }
            memcpy(openpgp_state.language, data, data_len);
            openpgp_state.language[data_len] = '\0';
            LOG_INFO("OpenPGP: Set language");
            return 0;

        case OPENPGP_DO_SEX:
            if (data_len != 1) {
                return -1;
            }
            openpgp_state.sex = data[0];
            LOG_INFO("OpenPGP: Set sex");
            return 0;

        case OPENPGP_DO_URL:
            if (data_len > OPENPGP_URL_MAX_LENGTH) {
                return -1;
            }
            memcpy(openpgp_state.url, data, data_len);
            openpgp_state.url[data_len] = '\0';
            LOG_INFO("OpenPGP: Set URL");
            return 0;

        default:
            return -1;
    }
}

int openpgp_reset(void)
{
    LOG_INFO("OpenPGP: Resetting to factory defaults");

    /* Reset PINs */
    memcpy(openpgp_state.pin_user, default_pin_user, sizeof(default_pin_user));
    openpgp_state.pin_user_len = sizeof(default_pin_user);
    memcpy(openpgp_state.pin_admin, default_pin_admin, sizeof(default_pin_admin));
    openpgp_state.pin_admin_len = sizeof(default_pin_admin);

    openpgp_state.pin_user_retries = OPENPGP_PIN_MAX_RETRIES;
    openpgp_state.pin_admin_retries = OPENPGP_PIN_MAX_RETRIES;
    openpgp_state.pin_user_verified = false;
    openpgp_state.pin_admin_verified = false;

    /* Clear keys */
    memset(key_slots, 0, sizeof(key_slots));

    /* Reset cardholder data */
    strcpy(openpgp_state.name, "OpenFIDO User");
    strcpy(openpgp_state.language, "en");
    openpgp_state.sex = '9';
    openpgp_state.url[0] = '\0';

    openpgp_state.sig_counter = 0;
    openpgp_state.terminated = false;

    return 0;
}
