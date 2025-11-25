/**
 * @file test_cbor.c
 * @brief Unit tests for CBOR encoder/decoder
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cbor.h"

/* Test helper macros */
#define TEST_ASSERT(condition)                                            \
    do {                                                                  \
        if (!(condition)) {                                               \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

#define TEST_PASS()                     \
    do {                                \
        printf("PASS: %s\n", __func__); \
        return 0;                       \
    } while (0)

/* Test encoding unsigned integers */
int test_cbor_encode_uint(void)
{
    uint8_t buffer[64];
    cbor_encoder_t encoder;

    /* Test small value (< 24) */
    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_encode_uint(&encoder, 10) == CBOR_OK);
    TEST_ASSERT(cbor_encoder_get_size(&encoder) == 1);
    TEST_ASSERT(buffer[0] == 0x0A);

    /* Test 1-byte value */
    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_encode_uint(&encoder, 100) == CBOR_OK);
    TEST_ASSERT(cbor_encoder_get_size(&encoder) == 2);
    TEST_ASSERT(buffer[0] == 0x18);
    TEST_ASSERT(buffer[1] == 100);

    /* Test 2-byte value */
    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_encode_uint(&encoder, 1000) == CBOR_OK);
    TEST_ASSERT(cbor_encoder_get_size(&encoder) == 3);
    TEST_ASSERT(buffer[0] == 0x19);

    TEST_PASS();
}

/* Test encoding byte strings */
int test_cbor_encode_bytes(void)
{
    uint8_t buffer[64];
    cbor_encoder_t encoder;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_encode_bytes(&encoder, data, sizeof(data)) == CBOR_OK);
    TEST_ASSERT(cbor_encoder_get_size(&encoder) == 5);
    TEST_ASSERT(buffer[0] == 0x44); /* byte string, length 4 */
    TEST_ASSERT(memcmp(&buffer[1], data, 4) == 0);

    TEST_PASS();
}

/* Test encoding maps */
int test_cbor_encode_map(void)
{
    uint8_t buffer[64];
    cbor_encoder_t encoder;

    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_encode_map_start(&encoder, 2) == CBOR_OK);

    /* Key 1 */
    TEST_ASSERT(cbor_encode_uint(&encoder, 1) == CBOR_OK);
    TEST_ASSERT(cbor_encode_text(&encoder, "test", 4) == CBOR_OK);

    /* Key 2 */
    TEST_ASSERT(cbor_encode_uint(&encoder, 2) == CBOR_OK);
    TEST_ASSERT(cbor_encode_bool(&encoder, true) == CBOR_OK);

    TEST_ASSERT(cbor_encoder_get_size(&encoder) > 0);

    TEST_PASS();
}

/* Test decoding unsigned integers */
int test_cbor_decode_uint(void)
{
    uint8_t buffer[] = {0x0A}; /* 10 */
    cbor_decoder_t decoder;
    uint64_t value;

    cbor_decoder_init(&decoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_decode_uint(&decoder, &value) == CBOR_OK);
    TEST_ASSERT(value == 10);

    TEST_PASS();
}

/* Test decoding byte strings */
int test_cbor_decode_bytes(void)
{
    uint8_t buffer[] = {0x44, 0x01, 0x02, 0x03, 0x04}; /* 4-byte string */
    cbor_decoder_t decoder;
    uint8_t data[10];
    size_t len = sizeof(data);

    cbor_decoder_init(&decoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_decode_bytes(&decoder, data, &len) == CBOR_OK);
    TEST_ASSERT(len == 4);
    TEST_ASSERT(data[0] == 0x01);
    TEST_ASSERT(data[3] == 0x04);

    TEST_PASS();
}

/* Test decoding maps */
int test_cbor_decode_map(void)
{
    uint8_t buffer[] = {0xA2, 0x01, 0x0A, 0x02, 0x14}; /* {1: 10, 2: 20} */
    cbor_decoder_t decoder;
    size_t count;
    uint64_t key, value;

    cbor_decoder_init(&decoder, buffer, sizeof(buffer));
    TEST_ASSERT(cbor_decode_map_start(&decoder, &count) == CBOR_OK);
    TEST_ASSERT(count == 2);

    /* First pair */
    TEST_ASSERT(cbor_decode_uint(&decoder, &key) == CBOR_OK);
    TEST_ASSERT(key == 1);
    TEST_ASSERT(cbor_decode_uint(&decoder, &value) == CBOR_OK);
    TEST_ASSERT(value == 10);

    /* Second pair */
    TEST_ASSERT(cbor_decode_uint(&decoder, &key) == CBOR_OK);
    TEST_ASSERT(key == 2);
    TEST_ASSERT(cbor_decode_uint(&decoder, &value) == CBOR_OK);
    TEST_ASSERT(value == 20);

    TEST_PASS();
}

/* Test roundtrip encoding/decoding */
int test_cbor_roundtrip(void)
{
    uint8_t buffer[128];
    cbor_encoder_t encoder;
    cbor_decoder_t decoder;

    /* Encode */
    cbor_encoder_init(&encoder, buffer, sizeof(buffer));
    cbor_encode_map_start(&encoder, 3);
    cbor_encode_uint(&encoder, 1);
    cbor_encode_uint(&encoder, 42);
    cbor_encode_uint(&encoder, 2);
    cbor_encode_text(&encoder, "hello", 5);
    cbor_encode_uint(&encoder, 3);
    cbor_encode_bool(&encoder, true);

    size_t encoded_size = cbor_encoder_get_size(&encoder);

    /* Decode */
    cbor_decoder_init(&decoder, buffer, encoded_size);
    size_t map_count;
    TEST_ASSERT(cbor_decode_map_start(&decoder, &map_count) == CBOR_OK);
    TEST_ASSERT(map_count == 3);

    uint64_t key, num_val;
    char text_val[10];
    size_t text_len = sizeof(text_val);
    bool bool_val;

    /* Entry 1 */
    TEST_ASSERT(cbor_decode_uint(&decoder, &key) == CBOR_OK);
    TEST_ASSERT(key == 1);
    TEST_ASSERT(cbor_decode_uint(&decoder, &num_val) == CBOR_OK);
    TEST_ASSERT(num_val == 42);

    /* Entry 2 */
    TEST_ASSERT(cbor_decode_uint(&decoder, &key) == CBOR_OK);
    TEST_ASSERT(key == 2);
    TEST_ASSERT(cbor_decode_text(&decoder, text_val, &text_len) == CBOR_OK);
    TEST_ASSERT(strcmp(text_val, "hello") == 0);

    /* Entry 3 */
    TEST_ASSERT(cbor_decode_uint(&decoder, &key) == CBOR_OK);
    TEST_ASSERT(key == 3);
    TEST_ASSERT(cbor_decode_bool(&decoder, &bool_val) == CBOR_OK);
    TEST_ASSERT(bool_val == true);

    TEST_PASS();
}

/* Run all CBOR tests */
int run_cbor_tests(void)
{
    int failures = 0;

    printf("\n=== Running CBOR Tests ===\n");

    failures += test_cbor_encode_uint();
    failures += test_cbor_encode_bytes();
    failures += test_cbor_encode_map();
    failures += test_cbor_decode_uint();
    failures += test_cbor_decode_bytes();
    failures += test_cbor_decode_map();
    failures += test_cbor_roundtrip();

    printf("=== CBOR Tests: %d failures ===\n\n", failures);
    return failures;
}
