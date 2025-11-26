/**
 * @file piv.c
 * @brief PIV (Personal Identity Verification) Core Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "piv.h"

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

/* PIV State */
static piv_state_t piv_state;
static piv_key_slot_t key_slots[4];
static piv_cert_slot_t cert_slots[4];

/* Default PIN: "123456" */
static const uint8_t default_pin[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36};
static const uint8_t default_puk[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};

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

    /* Check if PIN is being verified */
    if (cmd->lc == 0) {
        /* Return current PIN status */
        if (piv_state.pin_verified) {
            set_response_sw(resp, PIV_SW_SUCCESS);
        } else {
            set_response_sw(resp, PIV_SW_VERIFY_FAIL | piv_state.pin_retries);
        }
        resp->len = 0;
        return 0;
    }

    /* Verify PIN */
    if (pin_ref != PIV_PIN_APP && pin_ref != PIV_PIN_GLOBAL) {
        set_response_sw(resp, PIV_SW_INCORRECT_P1P2);
        resp->len = 0;
        return 0;
    }

    /* Check if PIN is blocked */
    if (piv_state.pin_retries == 0) {
        set_response_sw(resp, PIV_SW_AUTH_BLOCKED);
        resp->len = 0;
        return 0;
    }

    /* Compare PIN */
    if (cmd->lc == piv_state.pin_len && memcmp(cmd->data, piv_state.pin, piv_state.pin_len) == 0) {
        /* PIN correct */
        piv_state.pin_verified = true;
        piv_state.pin_retries = PIV_PIN_MAX_RETRIES;
        set_response_sw(resp, PIV_SW_SUCCESS);
        LOG_INFO("PIV: PIN verified successfully");
    } else {
        /* PIN incorrect */
        piv_state.pin_verified = false;
        piv_state.pin_retries--;
        set_response_sw(resp, PIV_SW_VERIFY_FAIL | piv_state.pin_retries);
        LOG_WARN("PIV: PIN verification failed, %d retries remaining", piv_state.pin_retries);
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

    if (pin_ref != PIV_PIN_APP && pin_ref != PIV_PIN_GLOBAL) {
        set_response_sw(resp, PIV_SW_INCORRECT_P1P2);
        resp->len = 0;
        return 0;
    }

    /* Must be authenticated */
    if (!piv_state.pin_verified) {
        set_response_sw(resp, PIV_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    /* Parse old and new PIN */
    if (cmd->lc < 12 || cmd->lc > 16) {
        set_response_sw(resp, PIV_SW_WRONG_LENGTH);
        resp->len = 0;
        return 0;
    }

    uint8_t old_len = cmd->data[0];
    const uint8_t *old_pin = &cmd->data[1];
    uint8_t new_len = cmd->data[1 + old_len];
    const uint8_t *new_pin = &cmd->data[2 + old_len];

    /* Verify old PIN */
    if (old_len != piv_state.pin_len || memcmp(old_pin, piv_state.pin, old_len) != 0) {
        set_response_sw(resp, PIV_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    /* Validate new PIN length */
    if (new_len < PIV_PIN_MIN_LENGTH || new_len > PIV_PIN_MAX_LENGTH) {
        set_response_sw(resp, PIV_SW_WRONG_LENGTH);
        resp->len = 0;
        return 0;
    }

    /* Set new PIN */
    memcpy(piv_state.pin, new_pin, new_len);
    piv_state.pin_len = new_len;

    set_response_sw(resp, PIV_SW_SUCCESS);
    resp->len = 0;

    LOG_INFO("PIV: PIN changed successfully");
    return 0;
}

/**
 * @brief Handle GET DATA command
 */
static int handle_get_data(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Parse object ID from data field (tag 0x5C) */
    if (cmd->lc < 2 || cmd->data[0] != 0x5C) {
        set_response_sw(resp, PIV_SW_WRONG_DATA);
        resp->len = 0;
        return 0;
    }

    uint32_t object_id = 0;
    uint8_t tag_len = cmd->data[1];

    if (tag_len == 1) {
        object_id = cmd->data[2];
    } else if (tag_len == 3) {
        object_id = (cmd->data[2] << 16) | (cmd->data[3] << 8) | cmd->data[4];
    } else {
        set_response_sw(resp, PIV_SW_WRONG_DATA);
        resp->len = 0;
        return 0;
    }

    LOG_DEBUG("PIV: GET DATA for object 0x%06X", object_id);

    /* Get data object */
    size_t data_len = 0;
    int ret = piv_get_data(object_id, resp->data, &data_len);

    if (ret == 0) {
        resp->len = data_len;
        set_response_sw(resp, PIV_SW_SUCCESS);
    } else {
        resp->len = 0;
        set_response_sw(resp, PIV_SW_FILE_NOT_FOUND);
    }

    return 0;
}

/**
 * @brief Handle PUT DATA command
 */
static int handle_put_data(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Must be authenticated */
    if (!piv_state.pin_verified) {
        set_response_sw(resp, PIV_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    /* Parse object ID from data field (tag 0x5C) */
    if (cmd->lc < 2 || cmd->data[0] != 0x5C) {
        set_response_sw(resp, PIV_SW_WRONG_DATA);
        resp->len = 0;
        return 0;
    }

    uint32_t object_id = 0;
    uint8_t tag_len = cmd->data[1];
    size_t data_offset = 2 + tag_len;

    if (tag_len == 1) {
        object_id = cmd->data[2];
    } else if (tag_len == 3) {
        object_id = (cmd->data[2] << 16) | (cmd->data[3] << 8) | cmd->data[4];
    } else {
        set_response_sw(resp, PIV_SW_WRONG_DATA);
        resp->len = 0;
        return 0;
    }

    /* Parse data (tag 0x53) */
    if (cmd->lc < data_offset + 2 || cmd->data[data_offset] != 0x53) {
        set_response_sw(resp, PIV_SW_WRONG_DATA);
        resp->len = 0;
        return 0;
    }

    uint8_t data_len = cmd->data[data_offset + 1];
    const uint8_t *data = &cmd->data[data_offset + 2];

    LOG_DEBUG("PIV: PUT DATA for object 0x%06X, len=%d", object_id, data_len);

    /* Store data object */
    int ret = piv_put_data(object_id, data, data_len);

    if (ret == 0) {
        set_response_sw(resp, PIV_SW_SUCCESS);
    } else {
        set_response_sw(resp, PIV_SW_DATA_INVALID);
    }

    resp->len = 0;
    return 0;
}

/**
 * @brief Handle GENERATE ASYMMETRIC KEY PAIR command
 */
static int handle_generate_asymmetric(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Must be authenticated */
    if (!piv_state.pin_verified) {
        set_response_sw(resp, PIV_SW_SECURITY_STATUS);
        resp->len = 0;
        return 0;
    }

    uint8_t key_ref = cmd->p2;
    uint8_t algorithm = PIV_ALG_ECC_P256; /* Default */

    /* Parse algorithm from data */
    if (cmd->lc >= 2 && cmd->data[0] == 0xAC) {
        /* Algorithm tag */
        if (cmd->data[2] == 0x80) {
            algorithm = cmd->data[4];
        }
    }

    LOG_INFO("PIV: Generate key pair for slot 0x%02X, algorithm 0x%02X", key_ref, algorithm);

    /* Generate key */
    size_t public_key_len = 0;
    int ret = piv_generate_key(key_ref, algorithm, resp->data, &public_key_len);

    if (ret == 0) {
        resp->len = public_key_len;
        set_response_sw(resp, PIV_SW_SUCCESS);
    } else {
        resp->len = 0;
        set_response_sw(resp, PIV_SW_DATA_INVALID);
    }

    return 0;
}

int piv_init(void)
{
    LOG_INFO("Initializing PIV module");

    memset(&piv_state, 0, sizeof(piv_state));
    memset(key_slots, 0, sizeof(key_slots));
    memset(cert_slots, 0, sizeof(cert_slots));

    /* Set default PIN and PUK */
    memcpy(piv_state.pin, default_pin, sizeof(default_pin));
    piv_state.pin_len = sizeof(default_pin);
    memcpy(piv_state.puk, default_puk, sizeof(default_puk));
    piv_state.puk_len = sizeof(default_puk);

    piv_state.pin_retries = PIV_PIN_MAX_RETRIES;
    piv_state.puk_retries = PIV_PUK_MAX_RETRIES;
    piv_state.pin_verified = false;

    /* Initialize key slots */
    key_slots[0].key_ref = PIV_KEY_AUTHENTICATION;
    key_slots[1].key_ref = PIV_KEY_SIGNATURE;
    key_slots[2].key_ref = PIV_KEY_KEY_MANAGEMENT;
    key_slots[3].key_ref = PIV_KEY_CARD_AUTH;

    /* Initialize cert slots */
    cert_slots[0].object_id = PIV_OBJ_AUTHENTICATION;
    cert_slots[1].object_id = PIV_OBJ_SIGNATURE;
    cert_slots[2].object_id = PIV_OBJ_KEY_MANAGEMENT;
    cert_slots[3].object_id = PIV_OBJ_CARD_AUTH;

    /* Register with CCID */
    usb_ccid_register_app((const uint8_t *) PIV_AID, PIV_AID_LEN, piv_handle_apdu);

    LOG_INFO("PIV initialized successfully");
    return 0;
}

int piv_handle_apdu(const void *cmd_ptr, void *resp_ptr)
{
    const apdu_command_t *cmd = (const apdu_command_t *) cmd_ptr;
    apdu_response_t *resp = (apdu_response_t *) resp_ptr;

    LOG_DEBUG("PIV: APDU INS=0x%02X P1=0x%02X P2=0x%02X Lc=%d", cmd->ins, cmd->p1, cmd->p2,
              cmd->lc);

    switch (cmd->ins) {
        case PIV_INS_VERIFY:
            return handle_verify(cmd, resp);

        case PIV_INS_CHANGE_REFERENCE:
            return handle_change_reference(cmd, resp);

        case PIV_INS_GET_DATA:
            return handle_get_data(cmd, resp);

        case PIV_INS_PUT_DATA:
            return handle_put_data(cmd, resp);

        case PIV_INS_GENERATE_ASYMMETRIC:
            return handle_generate_asymmetric(cmd, resp);

        case PIV_INS_GENERAL_AUTHENTICATE:
            /* TODO: Implement */
            set_response_sw(resp, PIV_SW_FUNC_NOT_SUPPORTED);
            resp->len = 0;
            return 0;

        default:
            LOG_WARN("PIV: Unsupported instruction 0x%02X", cmd->ins);
            set_response_sw(resp, PIV_SW_INS_NOT_SUPPORTED);
            resp->len = 0;
            return 0;
    }
}

int piv_verify_pin(uint8_t pin_ref, const uint8_t *pin, size_t pin_len)
{
    if (pin_ref != PIV_PIN_APP && pin_ref != PIV_PIN_GLOBAL) {
        return -1;
    }

    if (piv_state.pin_retries == 0) {
        return -2; /* Blocked */
    }

    if (pin_len == piv_state.pin_len && memcmp(pin, piv_state.pin, pin_len) == 0) {
        piv_state.pin_verified = true;
        piv_state.pin_retries = PIV_PIN_MAX_RETRIES;
        return 0;
    }

    piv_state.pin_verified = false;
    piv_state.pin_retries--;
    return -1;
}

int piv_change_pin(uint8_t pin_ref, const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
                   size_t new_len)
{
    if (!piv_state.pin_verified) {
        return -1;
    }

    if (old_len != piv_state.pin_len || memcmp(old_pin, piv_state.pin, old_len) != 0) {
        return -1;
    }

    if (new_len < PIV_PIN_MIN_LENGTH || new_len > PIV_PIN_MAX_LENGTH) {
        return -1;
    }

    memcpy(piv_state.pin, new_pin, new_len);
    piv_state.pin_len = new_len;

    return 0;
}

int piv_generate_key(uint8_t key_ref, uint8_t algorithm, uint8_t *public_key,
                     size_t *public_key_len)
{
    /* Find key slot */
    piv_key_slot_t *slot = NULL;
    for (int i = 0; i < 4; i++) {
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
    slot->generated = true;

    /* Return dummy public key for now */
    *public_key_len = 0;

    LOG_INFO("PIV: Generated key for slot 0x%02X", key_ref);
    return 0;
}

int piv_general_authenticate(uint8_t key_ref, uint8_t algorithm, const uint8_t *data,
                             size_t data_len, uint8_t *response, size_t *resp_len)
{
    /* TODO: Implement signature/decrypt operations */
    return -1;
}

int piv_get_data(uint32_t object_id, uint8_t *data, size_t *data_len)
{
    /* Find certificate slot */
    for (int i = 0; i < 4; i++) {
        if (cert_slots[i].object_id == object_id && cert_slots[i].present) {
            memcpy(data, cert_slots[i].cert, cert_slots[i].cert_len);
            *data_len = cert_slots[i].cert_len;
            return 0;
        }
    }

    /* Handle special objects */
    if (object_id == PIV_OBJ_DISCOVERY) {
        /* Return discovery object */
        *data_len = 0;
        return 0;
    }

    return -1;
}

int piv_put_data(uint32_t object_id, const uint8_t *data, size_t data_len)
{
    /* Find certificate slot */
    for (int i = 0; i < 4; i++) {
        if (cert_slots[i].object_id == object_id) {
            if (data_len > PIV_MAX_CERT_SIZE) {
                return -1;
            }

            memcpy(cert_slots[i].cert, data, data_len);
            cert_slots[i].cert_len = data_len;
            cert_slots[i].present = true;

            LOG_INFO("PIV: Stored certificate for object 0x%06X", object_id);
            return 0;
        }
    }

    return -1;
}

int piv_reset(void)
{
    LOG_INFO("PIV: Resetting to factory defaults");

    /* Reset state */
    memcpy(piv_state.pin, default_pin, sizeof(default_pin));
    piv_state.pin_len = sizeof(default_pin);
    memcpy(piv_state.puk, default_puk, sizeof(default_puk));
    piv_state.puk_len = sizeof(default_puk);

    piv_state.pin_retries = PIV_PIN_MAX_RETRIES;
    piv_state.puk_retries = PIV_PUK_MAX_RETRIES;
    piv_state.pin_verified = false;

    /* Clear keys and certificates */
    memset(key_slots, 0, sizeof(key_slots));
    memset(cert_slots, 0, sizeof(cert_slots));

    return 0;
}
