/**
 * @file ykman.h
 * @brief Yubico Management Protocol
 *
 * Proprietary protocol for Yubikey device configuration
 * Compatible with ykman (Yubikey Manager)
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef YKMAN_H
#define YKMAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Yubico Management AID */
#define YKMAN_AID "\xA0\x00\x00\x05\x27\x47\x11\x17"
#define YKMAN_AID_LEN 8

/* Management Instructions */
#define YKMAN_INS_READ_CONFIG 0x1D
#define YKMAN_INS_WRITE_CONFIG 0x1C
#define YKMAN_INS_SET_MODE 0x16
#define YKMAN_INS_GET_SERIAL 0x10
#define YKMAN_INS_GET_DEVICE_INFO 0x13

/* USB Interface Modes */
#define YKMAN_USB_MODE_OTP 0x01
#define YKMAN_USB_MODE_CCID 0x02
#define YKMAN_USB_MODE_FIDO 0x04
#define YKMAN_USB_MODE_FIDO2 0x08

/* Device Capabilities */
#define YKMAN_CAP_OTP 0x01
#define YKMAN_CAP_U2F 0x02
#define YKMAN_CAP_FIDO2 0x200
#define YKMAN_CAP_OATH 0x20
#define YKMAN_CAP_PIV 0x10
#define YKMAN_CAP_OPENPGP 0x08

/* Device Form Factors */
#define YKMAN_FORM_UNKNOWN 0x00
#define YKMAN_FORM_USB_A_KEYCHAIN 0x01
#define YKMAN_FORM_USB_A_NANO 0x02
#define YKMAN_FORM_USB_C_KEYCHAIN 0x03
#define YKMAN_FORM_USB_C_NANO 0x04

/**
 * @brief Device Configuration
 */
typedef struct {
    uint8_t usb_enabled;    /* Enabled USB interfaces */
    uint8_t usb_supported;  /* Supported USB interfaces */
    uint16_t capabilities;  /* Device capabilities */
    uint8_t form_factor;    /* Device form factor */
    uint32_t serial_number; /* Device serial number */
    uint8_t version_major;  /* Firmware version major */
    uint8_t version_minor;  /* Firmware version minor */
    uint8_t version_patch;  /* Firmware version patch */
} ykman_device_config_t;

/**
 * @brief Initialize Yubico Management module
 *
 * @return 0 on success, error code otherwise
 */
int ykman_init(void);

/**
 * @brief Handle Yubico Management APDU command
 *
 * @param cmd APDU command
 * @param resp APDU response
 * @return 0 on success, error code otherwise
 */
int ykman_handle_apdu(const void *cmd, void *resp);

/**
 * @brief Get device information
 *
 * @param config Output device configuration
 * @return 0 on success, error code otherwise
 */
int ykman_get_device_info(ykman_device_config_t *config);

/**
 * @brief Set USB interface mode
 *
 * @param mode USB mode flags
 * @return 0 on success, error code otherwise
 */
int ykman_set_usb_mode(uint8_t mode);

/**
 * @brief Get serial number
 *
 * @param serial Output serial number
 * @return 0 on success, error code otherwise
 */
int ykman_get_serial(uint32_t *serial);

#ifdef __cplusplus
}
#endif

#endif /* YKMAN_H */
