/**
 * @file usb_hid.h
 * @brief USB HID FIDO Interface
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB HID Return Codes */
#define USB_HID_OK 0
#define USB_HID_ERROR -1
#define USB_HID_ERROR_TIMEOUT -2

/* CTAPHID Constants */
#define CTAPHID_PACKET_SIZE 64
#define CTAPHID_INIT_PAYLOAD 57 /* 64 - 7 bytes header */
#define CTAPHID_CONT_PAYLOAD 59 /* 64 - 5 bytes header */

/* CTAPHID Commands */
#define CTAPHID_MSG 0x03
#define CTAPHID_CBOR 0x10
#define CTAPHID_INIT 0x06
#define CTAPHID_PING 0x01
#define CTAPHID_CANCEL 0x11
#define CTAPHID_ERROR 0x3F
#define CTAPHID_KEEPALIVE 0x3B

/**
 * @brief Initialize USB HID interface
 *
 * @return USB_HID_OK on success, error code otherwise
 */
int usb_hid_init(void);

/**
 * @brief Send data over USB HID
 *
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int usb_hid_send(const uint8_t *data, size_t len);

/**
 * @brief Receive data from USB HID
 *
 * @param data Pointer to receive buffer
 * @param max_len Maximum length to receive
 * @return Number of bytes received, or negative error code
 */
int usb_hid_receive(uint8_t *data, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_H */
