/**
 * @file permissions.h
 * @brief CTAP2 Permissions System
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Permission Bits (CTAP 2.1) */
#define PERM_MAKE_CREDENTIAL 0x01   /* MakeCredential (mc) */
#define PERM_GET_ASSERTION 0x02     /* GetAssertion (ga) */
#define PERM_CREDENTIAL_MGMT 0x04   /* CredentialManagement (cm) */
#define PERM_BIO_ENROLLMENT 0x08    /* BioEnrollment (be) */
#define PERM_LARGE_BLOB_WRITE 0x10  /* LargeBlobWrite (lbw) */
#define PERM_AUTHENTICATOR_CFG 0x20 /* AuthenticatorConfig (acfg) */

/* Permission State */
typedef struct {
    uint8_t permissions;    /* Active permissions bitmap */
    uint8_t rp_id_hash[32]; /* RP ID hash for MC/GA permissions */
    bool has_rp_id;         /* Whether RP ID is set */
    uint32_t expiry_time;   /* Permission expiry timestamp */
} permission_state_t;

/**
 * @brief Initialize permissions system
 */
void permissions_init(void);

/**
 * @brief Set permissions from PIN token
 *
 * @param permissions Permission bitmap
 * @param rp_id_hash RP ID hash (for MC/GA), can be NULL
 * @return true if successful
 */
bool permissions_set(uint8_t permissions, const uint8_t *rp_id_hash);

/**
 * @brief Check if a permission is granted
 *
 * @param permission Permission bit to check
 * @param rp_id_hash RP ID hash to verify (for MC/GA), can be NULL
 * @return true if permission is granted
 */
bool permissions_check(uint8_t permission, const uint8_t *rp_id_hash);

/**
 * @brief Clear all permissions
 */
void permissions_clear(void);

/**
 * @brief Get current permission state
 *
 * @return Pointer to permission state
 */
const permission_state_t *permissions_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSIONS_H */
