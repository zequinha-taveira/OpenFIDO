/**
 * @file cbor.c
 * @brief CBOR Encoding/Decoding Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "cbor.h"

#include <string.h>

/* Helper macros */
#define CBOR_MAJOR_TYPE(type) ((type) << 5)
#define CBOR_ADDITIONAL_INFO(info) ((info) & 0x1F)

/* ========== Encoder Implementation ========== */

void cbor_encoder_init(cbor_encoder_t *encoder, uint8_t *buffer, size_t size)
{
    encoder->buffer = buffer;
    encoder->buffer_size = size;
    encoder->offset = 0;
}

static int cbor_encode_type_value(cbor_encoder_t *encoder, uint8_t major_type, uint64_t value)
{
    if (encoder->offset >= encoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    if (value < 24) {
        encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(major_type) | value;
    } else if (value <= 0xFF) {
        if (encoder->offset + 2 > encoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(major_type) | 24;
        encoder->buffer[encoder->offset++] = value;
    } else if (value <= 0xFFFF) {
        if (encoder->offset + 3 > encoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(major_type) | 25;
        encoder->buffer[encoder->offset++] = (value >> 8) & 0xFF;
        encoder->buffer[encoder->offset++] = value & 0xFF;
    } else if (value <= 0xFFFFFFFF) {
        if (encoder->offset + 5 > encoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(major_type) | 26;
        encoder->buffer[encoder->offset++] = (value >> 24) & 0xFF;
        encoder->buffer[encoder->offset++] = (value >> 16) & 0xFF;
        encoder->buffer[encoder->offset++] = (value >> 8) & 0xFF;
        encoder->buffer[encoder->offset++] = value & 0xFF;
    } else {
        if (encoder->offset + 9 > encoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(major_type) | 27;
        for (int i = 7; i >= 0; i--) {
            encoder->buffer[encoder->offset++] = (value >> (i * 8)) & 0xFF;
        }
    }

    return CBOR_OK;
}

int cbor_encode_uint(cbor_encoder_t *encoder, uint64_t value)
{
    return cbor_encode_type_value(encoder, CBOR_TYPE_UNSIGNED, value);
}

int cbor_encode_int(cbor_encoder_t *encoder, int64_t value)
{
    if (value >= 0) {
        return cbor_encode_uint(encoder, value);
    } else {
        return cbor_encode_type_value(encoder, CBOR_TYPE_NEGATIVE, -1 - value);
    }
}

int cbor_encode_bytes(cbor_encoder_t *encoder, const uint8_t *data, size_t len)
{
    int ret = cbor_encode_type_value(encoder, CBOR_TYPE_BYTES, len);
    if (ret != CBOR_OK)
        return ret;

    if (encoder->offset + len > encoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    memcpy(&encoder->buffer[encoder->offset], data, len);
    encoder->offset += len;

    return CBOR_OK;
}

int cbor_encode_text(cbor_encoder_t *encoder, const char *text, size_t len)
{
    int ret = cbor_encode_type_value(encoder, CBOR_TYPE_TEXT, len);
    if (ret != CBOR_OK)
        return ret;

    if (encoder->offset + len > encoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    memcpy(&encoder->buffer[encoder->offset], text, len);
    encoder->offset += len;

    return CBOR_OK;
}

int cbor_encode_bool(cbor_encoder_t *encoder, bool value)
{
    if (encoder->offset >= encoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    encoder->buffer[encoder->offset++] =
        CBOR_MAJOR_TYPE(CBOR_TYPE_SIMPLE) | (value ? CBOR_TRUE : CBOR_FALSE);
    return CBOR_OK;
}

int cbor_encode_null(cbor_encoder_t *encoder)
{
    if (encoder->offset >= encoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    encoder->buffer[encoder->offset++] = CBOR_MAJOR_TYPE(CBOR_TYPE_SIMPLE) | CBOR_NULL;
    return CBOR_OK;
}

int cbor_encode_map_start(cbor_encoder_t *encoder, size_t count)
{
    return cbor_encode_type_value(encoder, CBOR_TYPE_MAP, count);
}

int cbor_encode_array_start(cbor_encoder_t *encoder, size_t count)
{
    return cbor_encode_type_value(encoder, CBOR_TYPE_ARRAY, count);
}

size_t cbor_encoder_get_size(const cbor_encoder_t *encoder)
{
    return encoder->offset;
}

/* ========== Decoder Implementation ========== */

void cbor_decoder_init(cbor_decoder_t *decoder, const uint8_t *buffer, size_t size)
{
    decoder->buffer = buffer;
    decoder->buffer_size = size;
    decoder->offset = 0;
}

static int cbor_decode_type_value(cbor_decoder_t *decoder, uint8_t expected_type, uint64_t *value)
{
    if (decoder->offset >= decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    uint8_t initial = decoder->buffer[decoder->offset++];
    uint8_t major_type = initial >> 5;
    uint8_t additional = initial & 0x1F;

    if (major_type != expected_type) {
        return CBOR_ERROR_INVALID;
    }

    if (additional < 24) {
        *value = additional;
    } else if (additional == 24) {
        if (decoder->offset >= decoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        *value = decoder->buffer[decoder->offset++];
    } else if (additional == 25) {
        if (decoder->offset + 2 > decoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        *value = ((uint64_t) decoder->buffer[decoder->offset] << 8) |
                 decoder->buffer[decoder->offset + 1];
        decoder->offset += 2;
    } else if (additional == 26) {
        if (decoder->offset + 4 > decoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        *value = ((uint64_t) decoder->buffer[decoder->offset] << 24) |
                 ((uint64_t) decoder->buffer[decoder->offset + 1] << 16) |
                 ((uint64_t) decoder->buffer[decoder->offset + 2] << 8) |
                 decoder->buffer[decoder->offset + 3];
        decoder->offset += 4;
    } else if (additional == 27) {
        if (decoder->offset + 8 > decoder->buffer_size)
            return CBOR_ERROR_OVERFLOW;
        *value = 0;
        for (int i = 0; i < 8; i++) {
            *value = (*value << 8) | decoder->buffer[decoder->offset++];
        }
    } else {
        return CBOR_ERROR_INVALID;
    }

    return CBOR_OK;
}

int cbor_decoder_get_type(cbor_decoder_t *decoder, uint8_t *type)
{
    if (decoder->offset >= decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    *type = decoder->buffer[decoder->offset] >> 5;
    return CBOR_OK;
}

int cbor_decode_uint(cbor_decoder_t *decoder, uint64_t *value)
{
    return cbor_decode_type_value(decoder, CBOR_TYPE_UNSIGNED, value);
}

int cbor_decode_int(cbor_decoder_t *decoder, int64_t *value)
{
    uint8_t type;
    cbor_decoder_get_type(decoder, &type);

    uint64_t uvalue;
    int ret;

    if (type == CBOR_TYPE_UNSIGNED) {
        ret = cbor_decode_uint(decoder, &uvalue);
        *value = (int64_t) uvalue;
    } else if (type == CBOR_TYPE_NEGATIVE) {
        ret = cbor_decode_type_value(decoder, CBOR_TYPE_NEGATIVE, &uvalue);
        *value = -1 - (int64_t) uvalue;
    } else {
        return CBOR_ERROR_INVALID;
    }

    return ret;
}

int cbor_decode_bytes(cbor_decoder_t *decoder, uint8_t *data, size_t *len)
{
    uint64_t byte_len;
    int ret = cbor_decode_type_value(decoder, CBOR_TYPE_BYTES, &byte_len);
    if (ret != CBOR_OK)
        return ret;

    if (decoder->offset + byte_len > decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    if (byte_len > *len) {
        return CBOR_ERROR_OVERFLOW;
    }

    memcpy(data, &decoder->buffer[decoder->offset], byte_len);
    decoder->offset += byte_len;
    *len = byte_len;

    return CBOR_OK;
}

int cbor_decode_text(cbor_decoder_t *decoder, char *text, size_t *len)
{
    uint64_t text_len;
    int ret = cbor_decode_type_value(decoder, CBOR_TYPE_TEXT, &text_len);
    if (ret != CBOR_OK)
        return ret;

    if (decoder->offset + text_len > decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    if (text_len > *len - 1) {
        return CBOR_ERROR_OVERFLOW;
    }

    memcpy(text, &decoder->buffer[decoder->offset], text_len);
    decoder->offset += text_len;
    text[text_len] = '\0';
    *len = text_len;

    return CBOR_OK;
}

int cbor_decode_bool(cbor_decoder_t *decoder, bool *value)
{
    if (decoder->offset >= decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    uint8_t byte = decoder->buffer[decoder->offset++];
    uint8_t major_type = byte >> 5;
    uint8_t simple_value = byte & 0x1F;

    if (major_type != CBOR_TYPE_SIMPLE) {
        return CBOR_ERROR_INVALID;
    }

    if (simple_value == CBOR_TRUE) {
        *value = true;
    } else if (simple_value == CBOR_FALSE) {
        *value = false;
    } else {
        return CBOR_ERROR_INVALID;
    }

    return CBOR_OK;
}

int cbor_decode_map_start(cbor_decoder_t *decoder, size_t *count)
{
    uint64_t map_count;
    int ret = cbor_decode_type_value(decoder, CBOR_TYPE_MAP, &map_count);
    if (ret != CBOR_OK)
        return ret;

    *count = (size_t) map_count;
    return CBOR_OK;
}

int cbor_decode_array_start(cbor_decoder_t *decoder, size_t *count)
{
    uint64_t array_count;
    int ret = cbor_decode_type_value(decoder, CBOR_TYPE_ARRAY, &array_count);
    if (ret != CBOR_OK)
        return ret;

    *count = (size_t) array_count;
    return CBOR_OK;
}

int cbor_decoder_skip(cbor_decoder_t *decoder)
{
    if (decoder->offset >= decoder->buffer_size) {
        return CBOR_ERROR_OVERFLOW;
    }

    uint8_t initial = decoder->buffer[decoder->offset];
    uint8_t major_type = initial >> 5;
    uint8_t additional = initial & 0x1F;

    /* Skip based on type */
    uint64_t value;
    int ret;

    switch (major_type) {
        case CBOR_TYPE_UNSIGNED:
        case CBOR_TYPE_NEGATIVE:
            ret = cbor_decode_type_value(decoder, major_type, &value);
            break;

        case CBOR_TYPE_BYTES:
        case CBOR_TYPE_TEXT:
            ret = cbor_decode_type_value(decoder, major_type, &value);
            if (ret == CBOR_OK) {
                decoder->offset += value;
            }
            break;

        case CBOR_TYPE_ARRAY:
        case CBOR_TYPE_MAP: {
            size_t count;
            if (major_type == CBOR_TYPE_ARRAY) {
                ret = cbor_decode_array_start(decoder, &count);
            } else {
                ret = cbor_decode_map_start(decoder, &count);
                count *= 2; /* Maps have key-value pairs */
            }
            if (ret == CBOR_OK) {
                for (size_t i = 0; i < count; i++) {
                    ret = cbor_decoder_skip(decoder);
                    if (ret != CBOR_OK)
                        break;
                }
            }
            break;
        }

        case CBOR_TYPE_SIMPLE:
            decoder->offset++;
            ret = CBOR_OK;
            break;

        default:
            ret = CBOR_ERROR_INVALID;
    }

    return ret;
}
