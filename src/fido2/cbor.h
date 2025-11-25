/**
 * @file cbor.h
 * @brief CBOR Encoding/Decoding for CTAP2
 *
 * Lightweight CBOR implementation for FIDO2 CTAP2 protocol messages.
 * Supports the subset of CBOR needed for CTAP2 communication.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef CBOR_H
#define CBOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CBOR Major Types */
#define CBOR_TYPE_UNSIGNED 0
#define CBOR_TYPE_NEGATIVE 1
#define CBOR_TYPE_BYTES 2
#define CBOR_TYPE_TEXT 3
#define CBOR_TYPE_ARRAY 4
#define CBOR_TYPE_MAP 5
#define CBOR_TYPE_TAG 6
#define CBOR_TYPE_SIMPLE 7

/* CBOR Simple Values */
#define CBOR_FALSE 20
#define CBOR_TRUE 21
#define CBOR_NULL 22
#define CBOR_UNDEFINED 23

/* Return Codes */
#define CBOR_OK 0
#define CBOR_ERROR -1
#define CBOR_ERROR_OVERFLOW -2
#define CBOR_ERROR_INVALID -3

/**
 * @brief CBOR Encoder Context
 */
typedef struct {
    uint8_t *buffer;    /**< Output buffer */
    size_t buffer_size; /**< Size of buffer */
    size_t offset;      /**< Current write offset */
} cbor_encoder_t;

/**
 * @brief CBOR Decoder Context
 */
typedef struct {
    const uint8_t *buffer; /**< Input buffer */
    size_t buffer_size;    /**< Size of buffer */
    size_t offset;         /**< Current read offset */
} cbor_decoder_t;

/* ========== Encoder Functions ========== */

/**
 * @brief Initialize CBOR encoder
 */
void cbor_encoder_init(cbor_encoder_t *encoder, uint8_t *buffer, size_t size);

/**
 * @brief Encode unsigned integer
 */
int cbor_encode_uint(cbor_encoder_t *encoder, uint64_t value);

/**
 * @brief Encode negative integer
 */
int cbor_encode_int(cbor_encoder_t *encoder, int64_t value);

/**
 * @brief Encode byte string
 */
int cbor_encode_bytes(cbor_encoder_t *encoder, const uint8_t *data, size_t len);

/**
 * @brief Encode text string
 */
int cbor_encode_text(cbor_encoder_t *encoder, const char *text, size_t len);

/**
 * @brief Encode boolean
 */
int cbor_encode_bool(cbor_encoder_t *encoder, bool value);

/**
 * @brief Encode null
 */
int cbor_encode_null(cbor_encoder_t *encoder);

/**
 * @brief Start encoding map
 */
int cbor_encode_map_start(cbor_encoder_t *encoder, size_t count);

/**
 * @brief Start encoding array
 */
int cbor_encode_array_start(cbor_encoder_t *encoder, size_t count);

/**
 * @brief Get encoded data size
 */
size_t cbor_encoder_get_size(const cbor_encoder_t *encoder);

/* ========== Decoder Functions ========== */

/**
 * @brief Initialize CBOR decoder
 */
void cbor_decoder_init(cbor_decoder_t *decoder, const uint8_t *buffer, size_t size);

/**
 * @brief Get next CBOR type
 */
int cbor_decoder_get_type(cbor_decoder_t *decoder, uint8_t *type);

/**
 * @brief Decode unsigned integer
 */
int cbor_decode_uint(cbor_decoder_t *decoder, uint64_t *value);

/**
 * @brief Decode signed integer
 */
int cbor_decode_int(cbor_decoder_t *decoder, int64_t *value);

/**
 * @brief Decode byte string
 */
int cbor_decode_bytes(cbor_decoder_t *decoder, uint8_t *data, size_t *len);

/**
 * @brief Decode text string
 */
int cbor_decode_text(cbor_decoder_t *decoder, char *text, size_t *len);

/**
 * @brief Decode boolean
 */
int cbor_decode_bool(cbor_decoder_t *decoder, bool *value);

/**
 * @brief Decode map start and get item count
 */
int cbor_decode_map_start(cbor_decoder_t *decoder, size_t *count);

/**
 * @brief Decode array start and get item count
 */
int cbor_decode_array_start(cbor_decoder_t *decoder, size_t *count);

/**
 * @brief Skip current CBOR item
 */
int cbor_decoder_skip(cbor_decoder_t *decoder);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_H */
