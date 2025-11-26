/**
 * @file storage.h
 * @brief Secure Credential Storage Interface
 *
 * This file defines the interface for secure storage of FIDO2 credentials,
 * PIN data, and other persistent authenticator state.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Storage Return Codes */
#define STORAGE_OK 0
#define STORAGE_ERROR -1
#define STORAGE_ERROR_NOT_FOUND -2
#define STORAGE_ERROR_FULL -3
#define STORAGE_ERROR_INVALID_PARAM -4
#define STORAGE_ERROR_CORRUPTED -5

/* Storage Limits */
#define STORAGE_MAX_CREDENTIALS 50
#define STORAGE_MAX_RP_ID_LENGTH 256
#define STORAGE_MAX_USER_ID_LENGTH 64
#define STORAGE_MAX_USER_NAME_LENGTH 64
#define STORAGE_MAX_DISPLAY_NAME_LENGTH 64
#define STORAGE_CREDENTIAL_ID_LENGTH 16

/* PIN Configuration */
#define STORAGE_PIN_MIN_LENGTH 4
#define STORAGE_PIN_MAX_LENGTH 63
#define STORAGE_PIN_MAX_RETRIES 8

/**
 * @brief Credential Descriptor
 */
typedef struct {
    uint8_t id[STORAGE_CREDENTIAL_ID_LENGTH];           /**< Credential ID */
    uint8_t rp_id_hash[32];                             /**< SHA-256 hash of RP ID */
    uint8_t user_id[STORAGE_MAX_USER_ID_LENGTH];        /**< User ID */
    size_t user_id_len;                                 /**< Length of user ID */
    uint8_t private_key[32];                            /**< ECDSA P-256 private key (encrypted) */
    uint32_t sign_count;                                /**< Signature counter */
    bool resident;                                      /**< Is this a resident key? */
    char user_name[STORAGE_MAX_USER_NAME_LENGTH];       /**< User name (for resident keys) */
    char display_name[STORAGE_MAX_DISPLAY_NAME_LENGTH]; /**< Display name (for resident keys) */
    char rp_id[STORAGE_MAX_RP_ID_LENGTH];               /**< RP ID (for resident keys) */
    int algorithm;                                      /**< COSE algorithm identifier */
    bool hmac_secret;                                   /**< HMAC-Secret extension enabled */
    uint8_t protection_policy;                          /**< Credential protection policy */
    uint8_t cred_blob[32];                              /**< credBlob extension data */
    size_t cred_blob_len;                               /**< Length of credBlob */
    uint8_t large_blob_key[32];                         /**< largeBlobKey for encryption */
    bool has_large_blob_key;                            /**< Whether largeBlobKey is set */
} storage_credential_t;

/**
 * @brief PIN Data Structure
 */
typedef struct {
    uint8_t pin_hash[32]; /**< SHA-256 hash of PIN */
    uint8_t pin_retries;  /**< Remaining PIN retries */
    bool pin_set;         /**< Is PIN set? */
} storage_pin_data_t;

/**
 * @brief Initialize storage subsystem
 *
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_init(void);

/**
 * @brief Format storage (erase all data)
 *
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_format(void);

/* ========== Credential Management ========== */

/**
 * @brief Store a credential
 *
 * @param credential Pointer to credential structure
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_store_credential(const storage_credential_t *credential);

/**
 * @brief Find credential by ID
 *
 * @param credential_id Credential ID to search for
 * @param credential Output credential structure
 * @return STORAGE_OK if found, STORAGE_ERROR_NOT_FOUND otherwise
 */
int storage_find_credential(const uint8_t *credential_id, storage_credential_t *credential);

/**
 * @brief Find credentials by RP ID hash
 *
 * @param rp_id_hash SHA-256 hash of RP ID
 * @param credentials Array to store found credentials
 * @param max_credentials Maximum number of credentials to return
 * @param count Output: number of credentials found
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_find_credentials_by_rp(const uint8_t *rp_id_hash, storage_credential_t *credentials,
                                   size_t max_credentials, size_t *count);

/**
 * @brief Update credential signature counter
 *
 * @param credential_id Credential ID
 * @param new_count New signature count
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_update_sign_count(const uint8_t *credential_id, uint32_t new_count);

/**
 * @brief Delete a credential
 *
 * @param credential_id Credential ID to delete
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_delete_credential(const uint8_t *credential_id);

/**
 * @brief Get number of stored credentials
 *
 * @param count Output: number of credentials
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_credential_count(size_t *count);

/**
 * @brief Get number of resident credentials
 *
 * @param count Output: number of resident credentials
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_resident_credential_count(size_t *count);

/* ========== PIN Management ========== */

/**
 * @brief Set PIN
 *
 * @param pin PIN string (UTF-8)
 * @param pin_len Length of PIN
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_set_pin(const uint8_t *pin, size_t pin_len);

/**
 * @brief Verify PIN
 *
 * @param pin PIN string to verify
 * @param pin_len Length of PIN
 * @return STORAGE_OK if correct, error code otherwise
 */
int storage_verify_pin(const uint8_t *pin, size_t pin_len);

/**
 * @brief Check if PIN is set
 *
 * @return true if PIN is set, false otherwise
 */
bool storage_is_pin_set(void);

/**
 * @brief Get remaining PIN retries
 *
 * @return Number of remaining retries, or -1 on error
 */
int storage_get_pin_retries(void);

/**
 * @brief Reset PIN retries counter
 *
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_reset_pin_retries(void);

/**
 * @brief Check if PIN is blocked
 *
 * @return true if blocked, false otherwise
 */
bool storage_is_pin_blocked(void);

/* ========== Global Counter ========== */

/**
 * @brief Get and increment global signature counter
 *
 * @param counter Output: current counter value
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_and_increment_counter(uint32_t *counter);

/**
 * @brief Get current global counter value (without incrementing)
 *
 * @param counter Output: current counter value
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_counter(uint32_t *counter);

/* ========== Attestation Key ========== */

/**
 * @brief Get attestation private key
 *
 * @param private_key Output: attestation private key (32 bytes)
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_attestation_key(uint8_t *private_key);

/**
 * @brief Set attestation private key
 *
 * @param private_key Attestation private key (32 bytes)
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_set_attestation_key(const uint8_t *private_key);

/**
 * @brief Get attestation certificate
 *
 * @param cert Output buffer for certificate
 * @param max_len Maximum length of buffer
 * @param cert_len Output: actual certificate length
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_get_attestation_cert(uint8_t *cert, size_t max_len, size_t *cert_len);

/**
 * @brief Set attestation certificate
 *
 * @param cert Certificate data
 * @param cert_len Length of certificate
 * @return STORAGE_OK on success, error code otherwise
 */
int storage_set_attestation_cert(const uint8_t *cert, size_t cert_len);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
