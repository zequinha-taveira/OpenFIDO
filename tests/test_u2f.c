/**
 * @file test_u2f.c
 * @brief Unit tests for U2F protocol
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "u2f.h"

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

/* Test U2F Version Command */
int test_u2f_version(void)
{
    uint8_t request[] = {0x00, U2F_VERSION, 0x00, 0x00};
    uint8_t response[256];
    size_t response_len = 0;

    uint16_t sw = u2f_process_apdu(request, sizeof(request), response, &response_len);

    TEST_ASSERT(sw == U2F_SW_NO_ERROR);
    TEST_ASSERT(response_len == 6);
    TEST_ASSERT(memcmp(response, "U2F_V2", 6) == 0);

    TEST_PASS();
}

/* Test U2F Version with Data (Should Fail) */
int test_u2f_version_invalid(void)
{
    uint8_t request[] = {0x00, U2F_VERSION, 0x00, 0x00, 0x01, 0x00}; /* Lc=1, Data=0x00 */
    uint8_t response[256];
    size_t response_len = 0;

    uint16_t sw = u2f_process_apdu(request, sizeof(request), response, &response_len);

    TEST_ASSERT(sw == U2F_SW_WRONG_DATA);

    TEST_PASS();
}

/* Test U2F Unknown INS */
int test_u2f_unknown_ins(void)
{
    uint8_t request[] = {0x00, 0xFF, 0x00, 0x00};
    uint8_t response[256];
    size_t response_len = 0;

    uint16_t sw = u2f_process_apdu(request, sizeof(request), response, &response_len);

    TEST_ASSERT(sw == U2F_SW_INS_NOT_SUPPORTED);

    TEST_PASS();
}

/* Test U2F Register Command */
int test_u2f_register(void)
{
    /* Initialize crypto and storage */
    crypto_init();
    storage_init();

    /* Build Register request: CLA INS P1 P2 Lc Challenge(32) Application(32) */
    uint8_t request[69];
    request[0] = 0x00; /* CLA */
    request[1] = U2F_REGISTER; /* INS */
    request[2] = 0x00; /* P1 */
    request[3] = 0x00; /* P2 */
    request[4] = 64; /* Lc */
    
    /* Challenge (32 bytes) */
    memset(&request[5], 0x01, 32);
    
    /* Application (32 bytes) */
    memset(&request[37], 0x02, 32);

    uint8_t response[1024];
    size_t response_len = 0;

    uint16_t sw = u2f_process_apdu(request, sizeof(request), response, &response_len);

    TEST_ASSERT(sw == U2F_SW_NO_ERROR);
    TEST_ASSERT(response_len > 66); /* At least: reserved(1) + pubkey(65) + handle_len(1) */
    TEST_ASSERT(response[0] == 0x05); /* Reserved byte */
    TEST_ASSERT(response[1] == 0x04); /* Uncompressed public key */

    TEST_PASS();
}

/* Test U2F Authenticate Command */
int test_u2f_authenticate(void)
{
    /* This test requires a registered credential first */
    /* For simplicity, we'll just test the error case with invalid key handle */
    
    uint8_t request[102];
    request[0] = 0x00; /* CLA */
    request[1] = U2F_AUTHENTICATE; /* INS */
    request[2] = U2F_AUTH_ENFORCE; /* P1 */
    request[3] = 0x00; /* P2 */
    request[4] = 97; /* Lc */
    
    /* Challenge (32 bytes) */
    memset(&request[5], 0x01, 32);
    
    /* Application (32 bytes) */
    memset(&request[37], 0x02, 32);
    
    /* Key handle length */
    request[69] = 32;
    
    /* Invalid key handle (32 bytes) */
    memset(&request[70], 0xFF, 32);

    uint8_t response[256];
    size_t response_len = 0;

    uint16_t sw = u2f_process_apdu(request, sizeof(request), response, &response_len);

    /* Should fail with wrong data since key handle doesn't exist */
    TEST_ASSERT(sw == U2F_SW_WRONG_DATA);

    TEST_PASS();
}

int main(void)
{
    int failures = 0;
    printf("\n=== Running U2F Tests ===\n");

    failures += test_u2f_version();
    failures += test_u2f_version_invalid();
    failures += test_u2f_unknown_ins();
    failures += test_u2f_register();
    failures += test_u2f_authenticate();

    printf("=== U2F Tests: %d failures ===\n\n", failures);
    return failures;
}
