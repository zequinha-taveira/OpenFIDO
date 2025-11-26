/**
 * @file ctap2_large_blobs.c
 * @brief CTAP2 Large Blobs Command Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <string.h>

#include "cbor.h"
#include "crypto.h"
#include "ctap2.h"
#include "logger.h"
#include "storage.h"

/* Large Blobs Subcommands */
#define LB_GET 0x01
#define LB_SET 0x02

/* Request Parameters */
#define LB_PARAM_GET 0x01
#define LB_PARAM_SET 0x02
#define LB_PARAM_OFFSET 0x03
#define LB_PARAM_LENGTH 0x04
#define LB_PARAM_PIN_UV_AUTH_PARAM 0x05
#define LB_PARAM_PIN_UV_AUTH_PROTOCOL 0x06

/* Response Parameters */
#define LB_RESP_CONFIG 0x01

/* Maximum large blob size */
#define MAX_LARGE_BLOB_SIZE 2048

/* Large blob storage */
static struct {
    uint8_t data[MAX_LARGE_BLOB_SIZE];
    size_t size;
    bool initialized;
} large_blob_storage = {0};

/**
 * @brief Initialize large blob storage
 */
static void init_large_blob_storage(void)
{
    if (!large_blob_storage.initialized) {
        /* Initialize with empty array: 0x80 (CBOR array of 0 items) + 16-byte hash */
        large_blob_storage.data[0] = 0x80; /* Empty CBOR array */

        /* Compute SHA-256 of empty array */
        uint8_t hash[32];
        crypto_sha256(large_blob_storage.data, 1, hash);
        memcpy(&large_blob_storage.data[1], hash, 16); /* First 16 bytes of hash */

        large_blob_storage.size = 17; /* 1 byte array + 16 bytes hash */
        large_blob_storage.initialized = true;

        LOG_INFO("Large blob storage initialized");
    }
}

/**
 * @brief Get large blob data
 */
static uint8_t lb_get(size_t offset, size_t length, uint8_t *response_data, size_t *response_len)
{
    init_large_blob_storage();

    if (offset > large_blob_storage.size) {
        return CTAP2_ERR_INVALID_PARAMETER;
    }

    size_t available = large_blob_storage.size - offset;
    size_t to_read = (length == 0 || length > available) ? available : length;

    cbor_encoder_t encoder;
    cbor_encoder_init(&encoder, response_data, CTAP2_MAX_MESSAGE_SIZE);

    cbor_encode_map_start(&encoder, 1);
    cbor_encode_uint(&encoder, LB_RESP_CONFIG);
    cbor_encode_bytes(&encoder, &large_blob_storage.data[offset], to_read);

    *response_len = cbor_encoder_get_size(&encoder);

    LOG_INFO("Large blob GET: offset=%zu, length=%zu, returned=%zu", offset, length, to_read);

    return CTAP2_OK;
}

/**
 * @brief Set large blob data
 */
static uint8_t lb_set(size_t offset, const uint8_t *data, size_t data_len, uint8_t *response_data,
                      size_t *response_len)
{
    init_large_blob_storage();

    if (offset + data_len > MAX_LARGE_BLOB_SIZE) {
        return CTAP2_ERR_REQUEST_TOO_LARGE;
    }

    /* Copy data to storage */
    memcpy(&large_blob_storage.data[offset], data, data_len);

    /* Update size if we extended the blob */
    if (offset + data_len > large_blob_storage.size) {
        large_blob_storage.size = offset + data_len;
    }

    *response_len = 0;

    LOG_INFO("Large blob SET: offset=%zu, length=%zu, total_size=%zu", offset, data_len,
             large_blob_storage.size);

    return CTAP2_OK;
}

/**
 * @brief Main large blobs command handler
 */
uint8_t ctap2_large_blobs(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                          size_t *response_len)
{
    LOG_INFO("Large blobs command");

    cbor_decoder_t decoder;
    cbor_decoder_init(&decoder, request_data, request_len);

    /* Parse request map */
    uint64_t map_size;
    if (cbor_decode_map_start(&decoder, &map_size) != CBOR_OK) {
        return CTAP2_ERR_INVALID_CBOR;
    }

    size_t offset = 0;
    size_t length = 0;
    uint8_t set_data[MAX_LARGE_BLOB_SIZE];
    size_t set_data_len = 0;
    bool is_get = false;
    bool is_set = false;

    /* Parse parameters */
    for (uint64_t i = 0; i < map_size; i++) {
        uint64_t key;
        if (cbor_decode_uint(&decoder, &key) != CBOR_OK) {
            return CTAP2_ERR_INVALID_CBOR;
        }

        switch (key) {
            case LB_PARAM_GET:
                if (cbor_decode_uint(&decoder, (uint64_t *) &length) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                is_get = true;
                break;

            case LB_PARAM_SET:
                set_data_len = sizeof(set_data);
                if (cbor_decode_bytes(&decoder, set_data, set_data_len) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                is_set = true;
                break;

            case LB_PARAM_OFFSET:
                if (cbor_decode_uint(&decoder, (uint64_t *) &offset) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            case LB_PARAM_LENGTH:
                if (cbor_decode_uint(&decoder, (uint64_t *) &length) != CBOR_OK) {
                    return CTAP2_ERR_INVALID_CBOR;
                }
                break;

            case LB_PARAM_PIN_UV_AUTH_PARAM:
            case LB_PARAM_PIN_UV_AUTH_PROTOCOL:
                /* Skip PIN auth for now */
                cbor_skip_value(&decoder);
                break;

            default:
                cbor_skip_value(&decoder);
                break;
        }
    }

    /* Execute subcommand */
    if (is_get) {
        return lb_get(offset, length, response_data, response_len);
    } else if (is_set) {
        return lb_set(offset, set_data, set_data_len, response_data, response_len);
    } else {
        return CTAP2_ERR_MISSING_PARAMETER;
    }
}
