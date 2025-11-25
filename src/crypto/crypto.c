/**
 * @file crypto.c
 * @brief Cryptographic Operations Implementation
 * 
 * Uses mbedTLS for cryptographic primitives
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "crypto.h"
#include "hal.h"
#include "logger.h"
#include "buffer.h"

#ifdef USE_MBEDTLS
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#endif

#include <string.h>

/* Global crypto context */
static struct {
    bool initialized;
#ifdef USE_MBEDTLS
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
#endif
} crypto_ctx = {0};

int crypto_init(void)
{
    LOG_INFO("Initializing cryptographic library");

#ifdef USE_MBEDTLS
    mbedtls_entropy_init(&crypto_ctx.entropy);
    mbedtls_ctr_drbg_init(&crypto_ctx.ctr_drbg);

    const char *pers = "openfido_fido2";
    int ret = mbedtls_ctr_drbg_seed(&crypto_ctx.ctr_drbg, mbedtls_entropy_func,
                                     &crypto_ctx.entropy,
                                     (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        LOG_ERROR("mbedTLS DRBG seed failed: %d", ret);
        return CRYPTO_ERROR;
    }
#endif

    crypto_ctx.initialized = true;
    LOG_INFO("Cryptographic library initialized");
    return CRYPTO_OK;
}

int crypto_deinit(void)
{
#ifdef USE_MBEDTLS
    mbedtls_ctr_drbg_free(&crypto_ctx.ctr_drbg);
    mbedtls_entropy_free(&crypto_ctx.entropy);
#endif
    crypto_ctx.initialized = false;
    return CRYPTO_OK;
}

int crypto_sha256(const uint8_t *data, size_t data_len, uint8_t *hash)
{
    if (!crypto_ctx.initialized || data == NULL || hash == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    int ret = mbedtls_sha256(data, data_len, hash, 0);
    if (ret != 0) {
        LOG_ERROR("SHA-256 failed: %d", ret);
        return CRYPTO_ERROR;
    }
#else
    /* Fallback: use HAL if available */
    if (hal_crypto_is_available()) {
        return (hal_crypto_sha256(data, data_len, hash) == HAL_OK) ? 
               CRYPTO_OK : CRYPTO_ERROR;
    }
    return CRYPTO_ERROR;
#endif

    return CRYPTO_OK;
}

int crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *hmac)
{
    if (!crypto_ctx.initialized || key == NULL || data == NULL || hmac == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int ret = mbedtls_md_hmac(md_info, key, key_len, data, data_len, hmac);
    if (ret != 0) {
        LOG_ERROR("HMAC-SHA256 failed: %d", ret);
        return CRYPTO_ERROR;
    }
#else
    return CRYPTO_ERROR;
#endif

    return CRYPTO_OK;
}

int crypto_ecdsa_generate_keypair(uint8_t *private_key, uint8_t *public_key)
{
    if (!crypto_ctx.initialized || private_key == NULL || public_key == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    /* Load P-256 curve */
    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        LOG_ERROR("Failed to load P-256 curve: %d", ret);
        goto cleanup;
    }

    /* Generate keypair */
    ret = mbedtls_ecdsa_genkey(&grp, &d, &Q, mbedtls_ctr_drbg_random, &crypto_ctx.ctr_drbg);
    if (ret != 0) {
        LOG_ERROR("ECDSA key generation failed: %d", ret);
        goto cleanup;
    }

    /* Export private key */
    ret = mbedtls_mpi_write_binary(&d, private_key, 32);
    if (ret != 0) {
        LOG_ERROR("Failed to export private key: %d", ret);
        goto cleanup;
    }

    /* Export public key (uncompressed format: X || Y) */
    ret = mbedtls_mpi_write_binary(&Q.X, &public_key[0], 32);
    if (ret != 0) {
        LOG_ERROR("Failed to export public key X: %d", ret);
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&Q.Y, &public_key[32], 32);
    if (ret != 0) {
        LOG_ERROR("Failed to export public key Y: %d", ret);
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);

    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash,
                      uint8_t *signature)
{
    if (!crypto_ctx.initialized || private_key == NULL || hash == NULL || signature == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_ecp_group grp;
    mbedtls_mpi d, r, s;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    /* Load P-256 curve */
    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        LOG_ERROR("Failed to load P-256 curve: %d", ret);
        goto cleanup;
    }

    /* Load private key */
    ret = mbedtls_mpi_read_binary(&d, private_key, 32);
    if (ret != 0) {
        LOG_ERROR("Failed to load private key: %d", ret);
        goto cleanup;
    }

    /* Sign */
    ret = mbedtls_ecdsa_sign(&grp, &r, &s, &d, hash, 32,
                             mbedtls_ctr_drbg_random, &crypto_ctx.ctr_drbg);
    if (ret != 0) {
        LOG_ERROR("ECDSA signing failed: %d", ret);
        goto cleanup;
    }

    /* Export signature (r || s) */
    ret = mbedtls_mpi_write_binary(&r, &signature[0], 32);
    if (ret != 0) {
        LOG_ERROR("Failed to export r: %d", ret);
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&s, &signature[32], 32);
    if (ret != 0) {
        LOG_ERROR("Failed to export s: %d", ret);
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    /* Try hardware acceleration */
    if (hal_crypto_is_available()) {
        return (hal_crypto_ecdsa_sign(private_key, hash, signature) == HAL_OK) ?
               CRYPTO_OK : CRYPTO_ERROR;
    }
    return CRYPTO_ERROR;
#endif
}

int crypto_ecdsa_verify(const uint8_t *public_key, const uint8_t *hash,
                        const uint8_t *signature)
{
    if (!crypto_ctx.initialized || public_key == NULL || hash == NULL || signature == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q;
    mbedtls_mpi r, s;

    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    /* Load P-256 curve */
    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto cleanup;

    /* Load public key */
    ret = mbedtls_mpi_read_binary(&Q.X, &public_key[0], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&Q.Y, &public_key[32], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_lset(&Q.Z, 1);
    if (ret != 0) goto cleanup;

    /* Load signature */
    ret = mbedtls_mpi_read_binary(&r, &signature[0], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&s, &signature[32], 32);
    if (ret != 0) goto cleanup;

    /* Verify */
    ret = mbedtls_ecdsa_verify(&grp, hash, 32, &Q, &r, &s);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_ecdsa_get_public_key(const uint8_t *private_key, uint8_t *public_key)
{
    if (!crypto_ctx.initialized || private_key == NULL || public_key == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&d, private_key, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, mbedtls_ctr_drbg_random, &crypto_ctx.ctr_drbg);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_write_binary(&Q.X, &public_key[0], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_write_binary(&Q.Y, &public_key[32], 32);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&Q);

    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_aes_gcm_encrypt(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *plaintext, size_t plaintext_len,
                           uint8_t *ciphertext, uint8_t *tag)
{
    if (!crypto_ctx.initialized || key == NULL || iv == NULL || 
        plaintext == NULL || ciphertext == NULL || tag == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        LOG_ERROR("GCM setkey failed: %d", ret);
        goto cleanup;
    }

    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len, iv, 12,
                                     aad, aad_len,
                                     plaintext, ciphertext,
                                     16, tag);
    if (ret != 0) {
        LOG_ERROR("GCM encrypt failed: %d", ret);
    }

cleanup:
    mbedtls_gcm_free(&gcm);
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_aes_gcm_decrypt(const uint8_t *key, const uint8_t *iv,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *ciphertext, size_t ciphertext_len,
                           const uint8_t *tag, uint8_t *plaintext)
{
    if (!crypto_ctx.initialized || key == NULL || iv == NULL ||
        ciphertext == NULL || tag == NULL || plaintext == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret != 0) {
        LOG_ERROR("GCM setkey failed: %d", ret);
        goto cleanup;
    }

    ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertext_len, iv, 12,
                                    aad, aad_len,
                                    tag, 16,
                                    ciphertext, plaintext);
    if (ret != 0) {
        LOG_ERROR("GCM decrypt failed: %d", ret);
    }

cleanup:
    mbedtls_gcm_free(&gcm);
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_random_generate(uint8_t *buffer, size_t len)
{
    if (!crypto_ctx.initialized || buffer == NULL || len == 0) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    int ret = mbedtls_ctr_drbg_random(&crypto_ctx.ctr_drbg, buffer, len);
    if (ret != 0) {
        LOG_ERROR("Random generation failed: %d", ret);
        return CRYPTO_ERROR;
    }
#else
    /* Use HAL RNG */
    if (hal_random_generate(buffer, len) != HAL_OK) {
        return CRYPTO_ERROR;
    }
#endif

    return CRYPTO_OK;
}

int crypto_ecdh_shared_secret(const uint8_t *private_key,
                               const uint8_t *peer_public_key,
                               uint8_t *shared_secret)
{
    if (!crypto_ctx.initialized || private_key == NULL || 
        peer_public_key == NULL || shared_secret == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point Q;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&Q);

    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&d, private_key, 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&Q.X, &peer_public_key[0], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_read_binary(&Q.Y, &peer_public_key[32], 32);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_lset(&Q.Z, 1);
    if (ret != 0) goto cleanup;

    ret = mbedtls_ecdh_compute_shared(&grp, &z, &Q, &d,
                                       mbedtls_ctr_drbg_random, &crypto_ctx.ctr_drbg);
    if (ret != 0) goto cleanup;

    ret = mbedtls_mpi_write_binary(&z, shared_secret, 32);

cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);
    mbedtls_ecp_point_free(&Q);

    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERROR;
#else
    return CRYPTO_ERROR;
#endif
}

int crypto_hkdf_sha256(const uint8_t *salt, size_t salt_len,
                       const uint8_t *ikm, size_t ikm_len,
                       const uint8_t *info, size_t info_len,
                       uint8_t *okm, size_t okm_len)
{
    if (!crypto_ctx.initialized || ikm == NULL || okm == NULL) {
        return CRYPTO_ERROR_INVALID_PARAM;
    }

#ifdef USE_MBEDTLS
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    int ret = mbedtls_hkdf(md, salt, salt_len, ikm, ikm_len, info, info_len, okm, okm_len);
    if (ret != 0) {
        LOG_ERROR("HKDF failed: %d", ret);
        return CRYPTO_ERROR;
    }
#else
    return CRYPTO_ERROR;
#endif

    return CRYPTO_OK;
}
