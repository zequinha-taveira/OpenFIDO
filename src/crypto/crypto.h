/**
 * @file crypto.h
 * @brief Cryptographic Operations Interface
 *
 * This file defines the cryptographic operations used by the FIDO2 authenticator.
 * Implementations can use software crypto (mbedTLS) or hardware acceleration.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Crypto Return Codes */
#define CRYPTO_OK 0
#define CRYPTO_ERROR -1
#define CRYPTO_ERROR_INVALID_PARAM -2
#define CRYPTO_ERROR_BUFFER_TOO_SMALL -3

/* Key and Hash Sizes */
#define CRYPTO_SHA256_DIGEST_SIZE 32
#define CRYPTO_P256_PRIVATE_KEY_SIZE 32
#define CRYPTO_P256_PUBLIC_KEY_SIZE 64 /* Uncompressed: 0x04 || X || Y */
#define CRYPTO_P256_SIGNATURE_SIZE 64  /* r || s */
#define CRYPTO_AES256_KEY_SIZE 32
#define CRYPTO_AES_BLOCK_SIZE 16
#define CRYPTO_AES_GCM_IV_SIZE 12
#define CRYPTO_AES_GCM_TAG_SIZE 16

/**
 * @brief Initialize cryptographic library
 *
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_init(void);

/**
 * @brief Deinitialize cryptographic library
 *
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_deinit(void);

/* ========== SHA-256 Functions ========== */

/**
 * @brief Compute SHA-256 hash
 *
 * @param data Input data
 * @param data_len Length of input data
 * @param hash Output hash (32 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_sha256(const uint8_t *data, size_t data_len, uint8_t *hash);

/* ========== HMAC-SHA256 Functions ========== */

/**
 * @brief Compute HMAC-SHA256
 *
 * @param key HMAC key
 * @param key_len Length of key
 * @param data Input data
 * @param data_len Length of input data
 * @param hmac Output HMAC (32 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                       uint8_t *hmac);

/* ========== ECDSA P-256 Functions ========== */

/**
 * @brief Generate ECDSA P-256 key pair
 *
 * @param private_key Output private key (32 bytes)
 * @param public_key Output public key (64 bytes, uncompressed)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ecdsa_generate_keypair(uint8_t *private_key, uint8_t *public_key);

/**
 * @brief Sign data with ECDSA P-256
 *
 * @param private_key Private key (32 bytes)
 * @param hash Message hash (32 bytes, typically SHA-256)
 * @param signature Output signature (64 bytes: r || s)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash, uint8_t *signature);

/**
 * @brief Verify ECDSA P-256 signature
 *
 * @param public_key Public key (64 bytes, uncompressed)
 * @param hash Message hash (32 bytes)
 * @param signature Signature to verify (64 bytes: r || s)
 * @return CRYPTO_OK if valid, error code otherwise
 */
int crypto_ecdsa_verify(const uint8_t *public_key, const uint8_t *hash, const uint8_t *signature);

/**
 * @brief Get public key from private key
 *
 * @param private_key Private key (32 bytes)
 * @param public_key Output public key (64 bytes, uncompressed)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ecdsa_get_public_key(const uint8_t *private_key, uint8_t *public_key);

/* ========== Ed25519 Functions ========== */

/**
 * @brief Generate Ed25519 key pair
 *
 * @param private_key Output private key (32 bytes)
 * @param public_key Output public key (32 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ed25519_generate_keypair(uint8_t *private_key, uint8_t *public_key);

/**
 * @brief Sign data with Ed25519
 *
 * @param private_key Private key (32 bytes)
 * @param message Message to sign
 * @param message_len Length of message
 * @param signature Output signature (64 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ed25519_sign(const uint8_t *private_key, const uint8_t *message, size_t message_len,
                        uint8_t *signature);

/**
 * @brief Verify Ed25519 signature
 *
 * @param public_key Public key (32 bytes)
 * @param message Message to verify
 * @param message_len Length of message
 * @param signature Signature to verify (64 bytes)
 * @return CRYPTO_OK if valid, error code otherwise
 */
int crypto_ed25519_verify(const uint8_t *public_key, const uint8_t *message, size_t message_len,
                          const uint8_t *signature);

/**
 * @brief Get public key from private key (Ed25519)
 *
 * @param private_key Private key (32 bytes)
 * @param public_key Output public key (32 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ed25519_get_public_key(const uint8_t *private_key, uint8_t *public_key);

/* ========== AES-256-GCM Functions ========== */

/**
 * @brief Encrypt data with AES-256-GCM
 *
 * @param key Encryption key (32 bytes)
 * @param iv Initialization vector (12 bytes)
 * @param aad Additional authenticated data (can be NULL)
 * @param aad_len Length of AAD
 * @param plaintext Input plaintext
 * @param plaintext_len Length of plaintext
 * @param ciphertext Output ciphertext (same length as plaintext)
 * @param tag Output authentication tag (16 bytes)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_aes_gcm_encrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *aad,
                           size_t aad_len, const uint8_t *plaintext, size_t plaintext_len,
                           uint8_t *ciphertext, uint8_t *tag);

/**
 * @brief Decrypt data with AES-256-GCM
 *
 * @param key Decryption key (32 bytes)
 * @param iv Initialization vector (12 bytes)
 * @param aad Additional authenticated data (can be NULL)
 * @param aad_len Length of AAD
 * @param ciphertext Input ciphertext
 * @param ciphertext_len Length of ciphertext
 * @param tag Authentication tag (16 bytes)
 * @param plaintext Output plaintext (same length as ciphertext)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_aes_gcm_decrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *aad,
                           size_t aad_len, const uint8_t *ciphertext, size_t ciphertext_len,
                           const uint8_t *tag, uint8_t *plaintext);

/* ========== Random Number Generation ========== */

/**
 * @brief Generate cryptographically secure random bytes
 *
 * @param buffer Output buffer
 * @param len Number of bytes to generate
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_random_generate(uint8_t *buffer, size_t len);

/* ========== ECDH P-256 Functions (for PIN protocol) ========== */

/**
 * @brief Perform ECDH key agreement
 *
 * @param private_key Our private key (32 bytes)
 * @param peer_public_key Peer's public key (64 bytes, uncompressed)
 * @param shared_secret Output shared secret (32 bytes, X coordinate)
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_ecdh_shared_secret(const uint8_t *private_key, const uint8_t *peer_public_key,
                              uint8_t *shared_secret);

/* ========== HKDF (for PIN protocol) ========== */

/**
 * @brief HKDF-SHA256 key derivation
 *
 * @param salt Salt value (can be NULL)
 * @param salt_len Length of salt
 * @param ikm Input keying material
 * @param ikm_len Length of IKM
 * @param info Context and application specific information (can be NULL)
 * @param info_len Length of info
 * @param okm Output keying material
 * @param okm_len Desired length of OKM
 * @return CRYPTO_OK on success, error code otherwise
 */
int crypto_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                       const uint8_t *info, size_t info_len, uint8_t *okm, size_t okm_len);

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
