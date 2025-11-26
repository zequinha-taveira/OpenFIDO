/**
 * @file ykman.c
 * @brief Yubico Management Protocol Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ykman.h"

#include <string.h>

#include "usb_ccid.h"
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

/* Device configuration */
static ykman_device_config_t device_config;

/**
 * @brief Build status word response
 */
static void set_response_sw(apdu_response_t *resp, uint16_t sw)
{
    resp->sw1 = (sw >> 8) & 0xFF;
    resp->sw2 = sw & 0xFF;
}

/**
 * @brief Handle GET DEVICE INFO command
 */
static int handle_get_device_info(const apdu_command_t *cmd, apdu_response_t *resp)
{
    LOG_DEBUG("YKMAN: Get device info");

    /* Build response */
    uint8_t *ptr = resp->data;

    /* Tag 0x01: Capabilities */
    *ptr++ = 0x01;
    *ptr++ = 0x02;
    *ptr++ = (device_config.capabilities >> 8) & 0xFF;
    *ptr++ = device_config.capabilities & 0xFF;

    /* Tag 0x02: Serial number */
    *ptr++ = 0x02;
    *ptr++ = 0x04;
    *ptr++ = (device_config.serial_number >> 24) & 0xFF;
    *ptr++ = (device_config.serial_number >> 16) & 0xFF;
    *ptr++ = (device_config.serial_number >> 8) & 0xFF;
    *ptr++ = device_config.serial_number & 0xFF;

    /* Tag 0x03: Version */
    *ptr++ = 0x03;
    *ptr++ = 0x03;
    *ptr++ = device_config.version_major;
    *ptr++ = device_config.version_minor;
    *ptr++ = device_config.version_patch;

    /* Tag 0x04: Form factor */
    *ptr++ = 0x04;
    *ptr++ = 0x01;
    *ptr++ = device_config.form_factor;

    /* Tag 0x05: USB supported */
    *ptr++ = 0x05;
    *ptr++ = 0x01;
    *ptr++ = device_config.usb_supported;

    /* Tag 0x06: USB enabled */
    *ptr++ = 0x06;
    *ptr++ = 0x01;
    *ptr++ = device_config.usb_enabled;

    resp->len = ptr - resp->data;
    set_response_sw(resp, 0x9000);

    return 0;
}

/**
 * @brief Handle GET SERIAL command
 */
static int handle_get_serial(const apdu_command_t *cmd, apdu_response_t *resp)
{
    LOG_DEBUG("YKMAN: Get serial number");

    resp->data[0] = (device_config.serial_number >> 24) & 0xFF;
    resp->data[1] = (device_config.serial_number >> 16) & 0xFF;
    resp->data[2] = (device_config.serial_number >> 8) & 0xFF;
    resp->data[3] = device_config.serial_number & 0xFF;

    resp->len = 4;
    set_response_sw(resp, 0x9000);

    return 0;
}

/**
 * @brief Handle SET MODE command
 */
static int handle_set_mode(const apdu_command_t *cmd, apdu_response_t *resp)
{
    if (cmd->lc < 1) {
        set_response_sw(resp, 0x6700);
        resp->len = 0;
        return 0;
    }

    uint8_t new_mode = cmd->data[0];

    LOG_INFO("YKMAN: Set USB mode to 0x%02X", new_mode);

    /* Validate mode */
    if ((new_mode & ~device_config.usb_supported) != 0) {
        /* Trying to enable unsupported interface */
        set_response_sw(resp, 0x6A80);
        resp->len = 0;
        return 0;
    }

    device_config.usb_enabled = new_mode;

    set_response_sw(resp, 0x9000);
    resp->len = 0;

    return 0;
}

/**
 * @brief Handle READ CONFIG command
 */
static int handle_read_config(const apdu_command_t *cmd, apdu_response_t *resp)
{
    LOG_DEBUG("YKMAN: Read config");

    /* Return current configuration */
    resp->data[0] = device_config.usb_enabled;
    resp->len = 1;

    set_response_sw(resp, 0x9000);

    return 0;
}

/**
 * @brief Handle WRITE CONFIG command
 */
static int handle_write_config(const apdu_command_t *cmd, apdu_response_t *resp)
{
    if (cmd->lc < 1) {
        set_response_sw(resp, 0x6700);
        resp->len = 0;
        return 0;
    }

    LOG_INFO("YKMAN: Write config");

    /* Update configuration */
    device_config.usb_enabled = cmd->data[0];

    set_response_sw(resp, 0x9000);
    resp->len = 0;

    return 0;
}

int ykman_init(void)
{
    LOG_INFO("Initializing Yubico Management module");

    memset(&device_config, 0, sizeof(device_config));

    /* Set device capabilities */
    device_config.capabilities = YKMAN_CAP_FIDO2 | YKMAN_CAP_U2F |
                                 YKMAN_CAP_PIV | YKMAN_CAP_OPENPGP |
                                 YKMAN_CAP_OATH | YKMAN_CAP_OTP;

    /* Set supported and enabled USB interfaces */
    device_config.usb_supported = YKMAN_USB_MODE_FIDO | YKMAN_USB_MODE_FIDO2 |
                                  YKMAN_USB_MODE_CCID | YKMAN_USB_MODE_OTP;
    device_config.usb_enabled = device_config.usb_supported;

    /* Set device info */
    device_config.form_factor = YKMAN_FORM_USB_A_KEYCHAIN;
    device_config.serial_number = 0x12345678; /* Should be unique */
    device_config.version_major = 5;
    device_config.version_minor = 4;
    device_config.version_patch = 3;

    /* Register with CCID */
    usb_ccid_register_app((const uint8_t *) YKMAN_AID, YKMAN_AID_LEN,
                          ykman_handle_apdu);

    LOG_INFO("Yubico Management initialized successfully");
    return 0;
}

int ykman_handle_apdu(const void *cmd_ptr, void *resp_ptr)
{
    const apdu_command_t *cmd = (const apdu_command_t *) cmd_ptr;
    apdu_response_t *resp = (apdu_response_t *) resp_ptr;

    LOG_DEBUG("YKMAN: APDU INS=0x%02X P1=0x%02X P2=0x%02X Lc=%d",
              cmd->ins, cmd->p1, cmd->p2, cmd->lc);

    switch (cmd->ins) {
    case YKMAN_INS_GET_DEVICE_INFO:
        return handle_get_device_info(cmd, resp);

    case YKMAN_INS_GET_SERIAL:
        return handle_get_serial(cmd, resp);

    case YKMAN_INS_SET_MODE:
        return handle_set_mode(cmd, resp);

    case YKMAN_INS_READ_CONFIG:
        return handle_read_config(cmd, resp);

    case YKMAN_INS_WRITE_CONFIG:
        return handle_write_config(cmd, resp);

    default:
        LOG_WARN("YKMAN: Unsupported instruction 0x%02X", cmd->ins);
        set_response_sw(resp, 0x6D00);
        resp->len = 0;
        return 0;
    }
}

int ykman_get_device_info(ykman_device_config_t *config)
{
    if (config == NULL) {
        return -1;
    }

    memcpy(config, &device_config, sizeof(ykman_device_config_t));
    return 0;
}

int ykman_set_usb_mode(uint8_t mode)
{
    if ((mode & ~device_config.usb_supported) != 0) {
        return -1;
    }

    device_config.usb_enabled = mode;
    LOG_INFO("YKMAN: USB mode set to 0x%02X", mode);

    return 0;
}

int ykman_get_serial(uint32_t *serial)
{
    if (serial == NULL) {
        return -1;
    }

    *serial = device_config.serial_number;
    return 0;
}
