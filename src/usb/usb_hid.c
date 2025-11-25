/**
 * @file usb_hid.c
 * @brief USB HID FIDO Interface Implementation
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "usb_hid.h"
#include "hal.h"
#include "logger.h"
#include <string.h>

/* CTAPHID packet structure */
typedef struct {
    uint32_t cid;           /* Channel ID */
    uint8_t cmd;            /* Command */
    uint8_t bcnth;          /* Byte count high */
    uint8_t bcntl;          /* Byte count low */
    uint8_t data[CTAPHID_INIT_PAYLOAD];
} ctaphid_init_packet_t;

typedef struct {
    uint32_t cid;           /* Channel ID */
    uint8_t seq;            /* Sequence number */
    uint8_t data[CTAPHID_CONT_PAYLOAD];
} ctaphid_cont_packet_t;

static uint32_t current_cid = 0;

int usb_hid_init(void)
{
    LOG_INFO("Initializing USB HID interface");
    
    /* Initialize USB hardware */
    int ret = hal_usb_init();
    if (ret != HAL_OK) {
        LOG_ERROR("USB initialization failed: %d", ret);
        return USB_HID_ERROR;
    }

    current_cid = 0xFFFFFFFF;  /* Broadcast channel */
    
    LOG_INFO("USB HID initialized successfully");
    return USB_HID_OK;
}

int usb_hid_send(const uint8_t *data, size_t len)
{
    if (len == 0 || data == NULL) {
        return USB_HID_ERROR;
    }

    /* Build initial packet */
    uint8_t packet[CTAPHID_PACKET_SIZE] = {0};
    ctaphid_init_packet_t *init_pkt = (ctaphid_init_packet_t *)packet;

    /* Set CID (big-endian) */
    init_pkt->cid = __builtin_bswap32(current_cid);
    
    /* Set command (CBOR) */
    init_pkt->cmd = CTAPHID_CBOR;
    
    /* Set byte count (big-endian) */
    init_pkt->bcnth = (len >> 8) & 0xFF;
    init_pkt->bcntl = len & 0xFF;

    /* Copy initial payload */
    size_t to_copy = (len < CTAPHID_INIT_PAYLOAD) ? len : CTAPHID_INIT_PAYLOAD;
    memcpy(init_pkt->data, data, to_copy);

    /* Send initial packet */
    if (hal_usb_send(packet, CTAPHID_PACKET_SIZE) != CTAPHID_PACKET_SIZE) {
        return USB_HID_ERROR;
    }

    size_t sent = to_copy;
    uint8_t seq = 0;

    /* Send continuation packets if needed */
    while (sent < len) {
        memset(packet, 0, sizeof(packet));
        ctaphid_cont_packet_t *cont_pkt = (ctaphid_cont_packet_t *)packet;

        cont_pkt->cid = __builtin_bswap32(current_cid);
        cont_pkt->seq = seq++;

        to_copy = ((len - sent) < CTAPHID_CONT_PAYLOAD) ? (len - sent) : CTAPHID_CONT_PAYLOAD;
        memcpy(cont_pkt->data, &data[sent], to_copy);

        if (hal_usb_send(packet, CTAPHID_PACKET_SIZE) != CTAPHID_PACKET_SIZE) {
            return USB_HID_ERROR;
        }

        sent += to_copy;
    }

    return sent;
}

int usb_hid_receive(uint8_t *data, size_t max_len)
{
    uint8_t packet[CTAPHID_PACKET_SIZE];
    
    /* Receive initial packet */
    int ret = hal_usb_receive(packet, CTAPHID_PACKET_SIZE, 0);
    if (ret <= 0) {
        return ret;
    }

    ctaphid_init_packet_t *init_pkt = (ctaphid_init_packet_t *)packet;

    /* Extract CID */
    current_cid = __builtin_bswap32(init_pkt->cid);

    /* Extract command */
    uint8_t cmd = init_pkt->cmd & 0x7F;

    /* Extract byte count */
    size_t total_len = ((size_t)init_pkt->bcnth << 8) | init_pkt->bcntl;

    if (total_len > max_len) {
        LOG_ERROR("Received data too large: %zu > %zu", total_len, max_len);
        return USB_HID_ERROR;
    }

    /* Handle INIT command specially */
    if (cmd == CTAPHID_INIT) {
        /* Return INIT response */
        uint8_t nonce[8];
        memcpy(nonce, init_pkt->data, 8);

        /* Generate new CID */
        current_cid = 0x12345678;  /* Should be random */

        /* Build INIT response */
        memset(packet, 0, sizeof(packet));
        init_pkt = (ctaphid_init_packet_t *)packet;
        init_pkt->cid = __builtin_bswap32(0xFFFFFFFF);
        init_pkt->cmd = CTAPHID_INIT;
        init_pkt->bcnth = 0;
        init_pkt->bcntl = 17;
        memcpy(&init_pkt->data[0], nonce, 8);
        init_pkt->data[8] = (current_cid >> 24) & 0xFF;
        init_pkt->data[9] = (current_cid >> 16) & 0xFF;
        init_pkt->data[10] = (current_cid >> 8) & 0xFF;
        init_pkt->data[11] = current_cid & 0xFF;
        init_pkt->data[12] = 2;  /* Protocol version */
        init_pkt->data[13] = 0;  /* Major device version */
        init_pkt->data[14] = 0;  /* Minor device version */
        init_pkt->data[15] = 0;  /* Build device version */
        init_pkt->data[16] = 0;  /* Capabilities */

        hal_usb_send(packet, CTAPHID_PACKET_SIZE);
        return 0;
    }

    /* Copy initial payload */
    size_t to_copy = (total_len < CTAPHID_INIT_PAYLOAD) ? total_len : CTAPHID_INIT_PAYLOAD;
    memcpy(data, init_pkt->data, to_copy);
    size_t received = to_copy;

    /* Receive continuation packets if needed */
    uint8_t expected_seq = 0;
    while (received < total_len) {
        ret = hal_usb_receive(packet, CTAPHID_PACKET_SIZE, 1000);
        if (ret <= 0) {
            return USB_HID_ERROR_TIMEOUT;
        }

        ctaphid_cont_packet_t *cont_pkt = (ctaphid_cont_packet_t *)packet;

        /* Verify CID */
        if (__builtin_bswap32(cont_pkt->cid) != current_cid) {
            LOG_ERROR("CID mismatch in continuation packet");
            return USB_HID_ERROR;
        }

        /* Verify sequence */
        if (cont_pkt->seq != expected_seq) {
            LOG_ERROR("Sequence error: expected %d, got %d", expected_seq, cont_pkt->seq);
            return USB_HID_ERROR;
        }

        to_copy = ((total_len - received) < CTAPHID_CONT_PAYLOAD) ? 
                  (total_len - received) : CTAPHID_CONT_PAYLOAD;
        memcpy(&data[received], cont_pkt->data, to_copy);
        received += to_copy;
        expected_seq++;
    }

    return received;
}
