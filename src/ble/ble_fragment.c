/**
 * @file ble_fragment.c
 * @brief BLE Message Fragmentation and Reassembly Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ble_fragment.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <string.h>

/* Static buffer for message reassembly */
static uint8_t reassembly_buffer[BLE_FRAGMENT_MAX_MESSAGE_SIZE];

/* ========== Fragment Buffer Management ========== */

void ble_fragment_init(ble_fragment_buffer_t *frag) {
    if (frag == NULL) {
        return;
    }

    frag->buffer = reassembly_buffer;
    frag->total_len = 0;
    frag->received_len = 0;
    frag->seq = 0;
    frag->in_progress = false;
}

int ble_fragment_add(ble_fragment_buffer_t *frag, const uint8_t *data, size_t len) {
    if (frag == NULL || data == NULL || len == 0) {
        return BLE_FRAGMENT_ERROR_INVALID_PARAM;
    }

    /* Check fragment type */
    uint8_t cmd = data[0];
    bool is_init = (cmd & BLE_FRAGMENT_TYPE_INIT) != 0;
    uint8_t seq = cmd & 0x7F;

    if (is_init) {
        /* Initial fragment */
        if (len < 3) {
            LOG_ERROR("INIT fragment too short: %zu bytes", len);
            return BLE_FRAGMENT_ERROR_INVALID_PARAM;
        }

        /* Extract total length (big-endian) */
        uint16_t total_len = ((uint16_t)data[1] << 8) | data[2];

        if (total_len > BLE_FRAGMENT_MAX_MESSAGE_SIZE) {
            LOG_ERROR("Message too large: %u bytes", total_len);
            return BLE_FRAGMENT_ERROR_TOO_LARGE;
        }

        /* Start new message */
        frag->total_len = total_len;
        frag->received_len = 0;
        frag->seq = 0;
        frag->in_progress = true;

        /* Copy data (skip cmd and length bytes) */
        size_t data_len = len - 3;
        if (data_len > 0) {
            if (data_len > frag->total_len) {
                data_len = frag->total_len;
            }
            memcpy(frag->buffer, &data[3], data_len);
            frag->received_len = data_len;
        }

        LOG_DEBUG("INIT fragment: total=%zu, received=%zu", frag->total_len, frag->received_len);
    } else {
        /* Continuation fragment */
        if (!frag->in_progress) {
            LOG_ERROR("CONT fragment without INIT");
            return BLE_FRAGMENT_ERROR_INVALID_SEQ;
        }

        /* Check sequence number */
        if (seq != frag->seq) {
            LOG_ERROR("Invalid sequence: expected %u, got %u", frag->seq, seq);
            return BLE_FRAGMENT_ERROR_INVALID_SEQ;
        }

        /* Copy data (skip cmd byte) */
        size_t data_len = len - 1;
        size_t remaining = frag->total_len - frag->received_len;

        if (data_len > remaining) {
            data_len = remaining;
        }

        memcpy(&frag->buffer[frag->received_len], &data[1], data_len);
        frag->received_len += data_len;

        LOG_DEBUG("CONT fragment seq=%u: received=%zu/%zu", seq, frag->received_len, frag->total_len);
    }

    /* Increment sequence number for next fragment */
    frag->seq = (frag->seq + 1) & 0x7F;

    return BLE_FRAGMENT_OK;
}

bool ble_fragment_is_complete(const ble_fragment_buffer_t *frag) {
    if (frag == NULL || !frag->in_progress) {
        return false;
    }

    return frag->received_len >= frag->total_len;
}

int ble_fragment_get_message(const ble_fragment_buffer_t *frag, uint8_t **data, size_t *len) {
    if (frag == NULL || data == NULL || len == NULL) {
        return BLE_FRAGMENT_ERROR_INVALID_PARAM;
    }

    if (!ble_fragment_is_complete(frag)) {
        return BLE_FRAGMENT_ERROR_INCOMPLETE;
    }

    *data = frag->buffer;
    *len = frag->total_len;

    return BLE_FRAGMENT_OK;
}

void ble_fragment_reset(ble_fragment_buffer_t *frag) {
    if (frag == NULL) {
        return;
    }

    frag->total_len = 0;
    frag->received_len = 0;
    frag->seq = 0;
    frag->in_progress = false;
}

/* ========== Fragment Creation ========== */

int ble_fragment_create(const uint8_t *data, size_t len, uint8_t *fragments[], 
                        size_t fragment_sizes[], size_t *num_fragments, size_t mtu) {
    if (data == NULL || fragments == NULL || fragment_sizes == NULL || num_fragments == NULL) {
        return BLE_FRAGMENT_ERROR_INVALID_PARAM;
    }

    if (len == 0 || len > BLE_FRAGMENT_MAX_MESSAGE_SIZE) {
        return BLE_FRAGMENT_ERROR_TOO_LARGE;
    }

    if (mtu < 3) {
        return BLE_FRAGMENT_ERROR_INVALID_PARAM;
    }

    size_t offset = 0;
    size_t frag_count = 0;
    uint8_t seq = 0;

    /* Create initial fragment */
    size_t init_payload_size = mtu - 3; /* cmd + 2 bytes length */
    size_t init_data_len = (len < init_payload_size) ? len : init_payload_size;

    fragments[frag_count] = malloc(3 + init_data_len);
    if (fragments[frag_count] == NULL) {
        return BLE_FRAGMENT_ERROR_NO_MEM;
    }

    /* INIT fragment: cmd | len_hi | len_lo | data */
    fragments[frag_count][0] = BLE_FRAGMENT_TYPE_INIT | seq;
    fragments[frag_count][1] = (len >> 8) & 0xFF;
    fragments[frag_count][2] = len & 0xFF;
    memcpy(&fragments[frag_count][3], data, init_data_len);
    fragment_sizes[frag_count] = 3 + init_data_len;

    offset += init_data_len;
    frag_count++;
    seq = (seq + 1) & 0x7F;

    /* Create continuation fragments */
    while (offset < len) {
        if (frag_count >= BLE_FRAGMENT_MAX_FRAGMENTS) {
            /* Clean up allocated fragments */
            for (size_t i = 0; i < frag_count; i++) {
                free(fragments[i]);
                fragments[i] = NULL;
            }
            return BLE_FRAGMENT_ERROR_TOO_LARGE;
        }

        size_t cont_payload_size = mtu - 1; /* cmd byte only */
        size_t cont_data_len = len - offset;
        if (cont_data_len > cont_payload_size) {
            cont_data_len = cont_payload_size;
        }

        fragments[frag_count] = malloc(1 + cont_data_len);
        if (fragments[frag_count] == NULL) {
            /* Clean up allocated fragments */
            for (size_t i = 0; i < frag_count; i++) {
                free(fragments[i]);
                fragments[i] = NULL;
            }
            return BLE_FRAGMENT_ERROR_NO_MEM;
        }

        /* CONT fragment: cmd | data */
        fragments[frag_count][0] = BLE_FRAGMENT_TYPE_CONT | seq;
        memcpy(&fragments[frag_count][1], &data[offset], cont_data_len);
        fragment_sizes[frag_count] = 1 + cont_data_len;

        offset += cont_data_len;
        frag_count++;
        seq = (seq + 1) & 0x7F;
    }

    *num_fragments = frag_count;

    LOG_DEBUG("Created %zu fragments for %zu byte message (MTU=%zu)", frag_count, len, mtu);

    return BLE_FRAGMENT_OK;
}
