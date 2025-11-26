/**
 * @file usb_ccid.h
 * @brief USB CCID (Chip Card Interface Device) Protocol
 *
 * Implements ISO/IEC 7816 smart card protocol over USB
 * Compatible with PC/SC and CCID specification rev 1.1
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef USB_CCID_H
#define USB_CCID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CCID Message Types (PC_to_RDR) */
#define CCID_PC_TO_RDR_ICCPOWERON 0x62
#define CCID_PC_TO_RDR_ICCPOWEROFF 0x63
#define CCID_PC_TO_RDR_GETSLOTSTATUS 0x65
#define CCID_PC_TO_RDR_XFRBLOCK 0x6F
#define CCID_PC_TO_RDR_GETPARAMETERS 0x6C
#define CCID_PC_TO_RDR_RESETPARAMETERS 0x6D
#define CCID_PC_TO_RDR_SETPARAMETERS 0x61
#define CCID_PC_TO_RDR_ESCAPE 0x6B
#define CCID_PC_TO_RDR_ICCCLOCK 0x6E
#define CCID_PC_TO_RDR_ABORT 0x6F
#define CCID_PC_TO_RDR_SECURE 0x69

/* CCID Message Types (RDR_to_PC) */
#define CCID_RDR_TO_PC_DATABLOCK 0x80
#define CCID_RDR_TO_PC_SLOTSTATUS 0x81
#define CCID_RDR_TO_PC_PARAMETERS 0x82
#define CCID_RDR_TO_PC_ESCAPE 0x83
#define CCID_RDR_TO_PC_DATARATEANDCLOCK 0x84

/* CCID Slot Status */
#define CCID_ICC_PRESENT_ACTIVE 0x00
#define CCID_ICC_PRESENT_INACTIVE 0x01
#define CCID_ICC_NOT_PRESENT 0x02

/* CCID Command Status */
#define CCID_CMD_STATUS_OK 0x00
#define CCID_CMD_STATUS_FAILED 0x40
#define CCID_CMD_STATUS_TIME_EXT 0x80

/* CCID Error Codes */
#define CCID_ERROR_CMD_ABORTED 0xFF
#define CCID_ERROR_ICC_MUTE 0xFE
#define CCID_ERROR_XFR_PARITY_ERROR 0xFD
#define CCID_ERROR_XFR_OVERRUN 0xFC
#define CCID_ERROR_HW_ERROR 0xFB
#define CCID_ERROR_BAD_ATR_TS 0xF8
#define CCID_ERROR_BAD_ATR_TCK 0xF7
#define CCID_ERROR_ICC_PROTOCOL_NOT_SUPPORTED 0xF6
#define CCID_ERROR_ICC_CLASS_NOT_SUPPORTED 0xF5
#define CCID_ERROR_PROCEDURE_BYTE_CONFLICT 0xF4
#define CCID_ERROR_DEACTIVATED_PROTOCOL 0xF3
#define CCID_ERROR_BUSY_WITH_AUTO_SEQUENCE 0xF2
#define CCID_ERROR_PIN_TIMEOUT 0xF0
#define CCID_ERROR_PIN_CANCELLED 0xEF
#define CCID_ERROR_CMD_SLOT_BUSY 0xE0
#define CCID_ERROR_CMD_NOT_SUPPORTED 0x00

/* APDU Constants */
#define APDU_MAX_LENGTH 261          /* 4 byte header + 1 Lc + 255 data + 1 Le */
#define APDU_RESPONSE_MAX_LENGTH 258 /* 256 data + 2 status bytes */

/* CCID Packet Size */
#define CCID_PACKET_SIZE 64
#define CCID_HEADER_SIZE 10

/* ATR (Answer To Reset) */
#define ATR_MAX_LENGTH 33

/**
 * @brief CCID Message Header
 */
typedef struct __attribute__((packed)) {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bSpecific[3];
} ccid_header_t;

/**
 * @brief APDU Command Structure (ISO 7816-4)
 */
typedef struct {
    uint8_t cla;       /* Class byte */
    uint8_t ins;       /* Instruction byte */
    uint8_t p1;        /* Parameter 1 */
    uint8_t p2;        /* Parameter 2 */
    uint8_t lc;        /* Length of command data */
    uint8_t data[255]; /* Command data */
    uint8_t le;        /* Expected response length */
} apdu_command_t;

/**
 * @brief APDU Response Structure
 */
typedef struct {
    uint8_t data[256]; /* Response data */
    uint16_t len;      /* Length of response data */
    uint8_t sw1;       /* Status word 1 */
    uint8_t sw2;       /* Status word 2 */
} apdu_response_t;

/**
 * @brief APDU Handler Function Type
 *
 * @param cmd APDU command
 * @param resp APDU response
 * @return 0 on success, error code otherwise
 */
typedef int (*apdu_handler_t)(const apdu_command_t *cmd, apdu_response_t *resp);

/**
 * @brief Initialize CCID interface
 *
 * @return 0 on success, error code otherwise
 */
int usb_ccid_init(void);

/**
 * @brief Process CCID message from host
 *
 * @param data Input message buffer
 * @param len Length of input message
 * @param response Output response buffer
 * @param resp_len Output: length of response
 * @return 0 on success, error code otherwise
 */
int usb_ccid_process_message(const uint8_t *data, size_t len, uint8_t *response, size_t *resp_len);

/**
 * @brief Register APDU handler for specific application
 *
 * @param aid Application Identifier (AID)
 * @param aid_len Length of AID
 * @param handler Handler function
 * @return 0 on success, error code otherwise
 */
int usb_ccid_register_app(const uint8_t *aid, size_t aid_len, apdu_handler_t handler);

/**
 * @brief Get ATR (Answer To Reset)
 *
 * @param atr Output buffer for ATR
 * @param len Output: length of ATR
 * @return 0 on success, error code otherwise
 */
int usb_ccid_get_atr(uint8_t *atr, size_t *len);

/**
 * @brief Set card present/absent status
 *
 * @param present true if card present, false otherwise
 */
void usb_ccid_set_card_present(bool present);

/**
 * @brief Get current slot status
 *
 * @return CCID slot status code
 */
uint8_t usb_ccid_get_slot_status(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_CCID_H */
