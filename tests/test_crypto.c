/**
 * @file test_crypto.c
 * @brief Unit tests for cryptographic operations
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "crypto.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return 1; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 0; \
    } while(0)

/* Test SHA-256 */
int test_crypto_sha256(void)
{
    uint8_t data[] = "test";
    uint8_t hash[32];
    
    /* Known SHA-256 hash of "test" */
    uint8_t expected[32] = {
        0x9f, 0x86, 0xd0, 0x81, 0x88, 0x4c, 0x7d, 0x65,
        0x9a, 0x2f, 0xea, 0xa0, 0xc5, 0x5a, 0xd0, 0x15,
        0xa3, 0xbf, 0x4f, 0x1b, 0x2b, 0x0b, 0x82, 0x2c,
        0xd1, 0x5d, 0x6c, 0x15, 0xb0, 0xf0, 0x0a, 0x08
    };

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    TEST_ASSERT(crypto_sha256(data, 4, hash) == CRYPTO_OK);
    TEST_ASSERT(memcmp(hash, expected, 32) == 0);

    TEST_PASS();
}

/* Test ECDSA key generation */
int test_crypto_ecdsa_keygen(void)
{
    uint8_t private_key[32];
    uint8_t public_key[64];

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    TEST_ASSERT(crypto_ecdsa_generate_keypair(private_key, public_key) == CRYPTO_OK);

    /* Verify keys are not all zeros */
    int all_zero = 1;
    for (int i = 0; i < 32; i++) {
        if (private_key[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero == 0);

    TEST_PASS();
}

/* Test ECDSA sign and verify */
int test_crypto_ecdsa_sign_verify(void)
{
    uint8_t private_key[32];
    uint8_t public_key[64];
    uint8_t hash[32] = {0x01, 0x02, 0x03}; /* Dummy hash */
    uint8_t signature[64];

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    TEST_ASSERT(crypto_ecdsa_generate_keypair(private_key, public_key) == CRYPTO_OK);
    TEST_ASSERT(crypto_ecdsa_sign(private_key, hash, signature) == CRYPTO_OK);
    TEST_ASSERT(crypto_ecdsa_verify(public_key, hash, signature) == CRYPTO_OK);

    /* Verify with wrong hash should fail */
    hash[0] = 0xFF;
    TEST_ASSERT(crypto_ecdsa_verify(public_key, hash, signature) != CRYPTO_OK);

    TEST_PASS();
}

/* Test AES-GCM encryption/decryption */
int test_crypto_aes_gcm(void)
{
    uint8_t key[32] = {0};
    uint8_t iv[12] = {0};
    uint8_t plaintext[] = "Hello, FIDO2!";
    uint8_t ciphertext[32];
    uint8_t decrypted[32];
    uint8_t tag[16];

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    
    /* Encrypt */
    TEST_ASSERT(crypto_aes_gcm_encrypt(key, iv, NULL, 0,
                                       plaintext, sizeof(plaintext),
                                       ciphertext, tag) == CRYPTO_OK);

    /* Decrypt */
    TEST_ASSERT(crypto_aes_gcm_decrypt(key, iv, NULL, 0,
                                       ciphertext, sizeof(plaintext),
                                       tag, decrypted) == CRYPTO_OK);

    /* Verify */
    TEST_ASSERT(memcmp(plaintext, decrypted, sizeof(plaintext)) == 0);

    TEST_PASS();
}

/* Test random generation */
int test_crypto_random(void)
{
    uint8_t buffer1[32];
    uint8_t buffer2[32];

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    TEST_ASSERT(crypto_random_generate(buffer1, sizeof(buffer1)) == CRYPTO_OK);
    TEST_ASSERT(crypto_random_generate(buffer2, sizeof(buffer2)) == CRYPTO_OK);

    /* Two random buffers should be different */
    TEST_ASSERT(memcmp(buffer1, buffer2, 32) != 0);

    TEST_PASS();
}

/* Test HMAC-SHA256 */
int test_crypto_hmac(void)
{
    uint8_t key[] = "secret";
    uint8_t data[] = "message";
    uint8_t hmac[32];

    TEST_ASSERT(crypto_init() == CRYPTO_OK);
    TEST_ASSERT(crypto_hmac_sha256(key, sizeof(key) - 1,
                                   data, sizeof(data) - 1,
                                   hmac) == CRYPTO_OK);

    /* HMAC should not be all zeros */
    int all_zero = 1;
    for (int i = 0; i < 32; i++) {
        if (hmac[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero == 0);

    TEST_PASS();
}

/* Run all crypto tests */
int run_crypto_tests(void)
{
    int failures = 0;

    printf("\n=== Running Crypto Tests ===\n");

    failures += test_crypto_sha256();
    failures += test_crypto_ecdsa_keygen();
    failures += test_crypto_ecdsa_sign_verify();
    failures += test_crypto_aes_gcm();
    failures += test_crypto_random();
    failures += test_crypto_hmac();

    printf("=== Crypto Tests: %d failures ===\n\n", failures);
    return failures;
}
