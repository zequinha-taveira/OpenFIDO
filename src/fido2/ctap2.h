/**
 * @file ctap2.h
 * @brief FIDO2 CTAP2 Protocol Implementation
 *
 * This file contains the main CTAP2 protocol definitions and functions
 * for implementing a FIDO2 authenticator.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef CTAP2_H
#define CTAP2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CTAP2 Command Codes */
#define CTAP2_CMD_MAKE_CREDENTIAL 0x01
#define CTAP2_CMD_GET_ASSERTION 0x02
#define CTAP2_CMD_GET_INFO 0x04
#define CTAP2_CMD_CLIENT_PIN 0x06
#define CTAP2_CMD_RESET 0x07
#define CTAP2_CMD_GET_NEXT_ASSERTION 0x08
#define CTAP2_CMD_BIO_ENROLLMENT 0x09
#define CTAP2_CMD_CREDENTIAL_MANAGEMENT 0x0A
#define CTAP2_CMD_SELECTION 0x0B
#define CTAP2_CMD_LARGE_BLOBS 0x0C
#define CTAP2_CMD_CONFIG 0x0D

/* CTAP2 Status Codes */
#define CTAP2_OK 0x00
#define CTAP2_ERR_INVALID_COMMAND 0x01
#define CTAP2_ERR_INVALID_PARAMETER 0x02
#define CTAP2_ERR_INVALID_LENGTH 0x03
#define CTAP2_ERR_INVALID_SEQ 0x04
#define CTAP2_ERR_TIMEOUT 0x05
#define CTAP2_ERR_CHANNEL_BUSY 0x06
#define CTAP2_ERR_LOCK_REQUIRED 0x0A
#define CTAP2_ERR_INVALID_CHANNEL 0x0B
#define CTAP2_ERR_CBOR_UNEXPECTED_TYPE 0x11
#define CTAP2_ERR_INVALID_CBOR 0x12
#define CTAP2_ERR_MISSING_PARAMETER 0x14
#define CTAP2_ERR_LIMIT_EXCEEDED 0x15
#define CTAP2_ERR_UNSUPPORTED_EXTENSION 0x16
#define CTAP2_ERR_CREDENTIAL_EXCLUDED 0x19
#define CTAP2_ERR_PROCESSING 0x21
#define CTAP2_ERR_INVALID_CREDENTIAL 0x22
#define CTAP2_ERR_USER_ACTION_PENDING 0x23
#define CTAP2_ERR_OPERATION_PENDING 0x24
#define CTAP2_ERR_NO_OPERATIONS 0x25
#define CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26
#define CTAP2_ERR_OPERATION_DENIED 0x27
#define CTAP2_ERR_KEY_STORE_FULL 0x28
#define CTAP2_ERR_NO_OPERATION_PENDING 0x2A
#define CTAP2_ERR_UNSUPPORTED_OPTION 0x2B
#define CTAP2_ERR_INVALID_OPTION 0x2C
#define CTAP2_ERR_KEEPALIVE_CANCEL 0x2D
#define CTAP2_ERR_NO_CREDENTIALS 0x2E
#define CTAP2_ERR_USER_ACTION_TIMEOUT 0x2F
#define CTAP2_ERR_NOT_ALLOWED 0x30
#define CTAP2_ERR_PIN_INVALID 0x31
#define CTAP2_ERR_PIN_BLOCKED 0x32
#define CTAP2_ERR_PIN_AUTH_INVALID 0x33
#define CTAP2_ERR_PIN_AUTH_BLOCKED 0x34
#define CTAP2_ERR_PIN_NOT_SET 0x35
#define CTAP2_ERR_PIN_REQUIRED 0x36
#define CTAP2_ERR_PIN_POLICY_VIOLATION 0x37
#define CTAP2_ERR_PIN_TOKEN_EXPIRED 0x38
#define CTAP2_ERR_REQUEST_TOO_LARGE 0x39
#define CTAP2_ERR_ACTION_TIMEOUT 0x3A
#define CTAP2_ERR_UP_REQUIRED 0x3B
#define CTAP2_ERR_UV_BLOCKED 0x3C
#define CTAP2_ERR_INTEGRITY_FAILURE 0x3D
#define CTAP2_ERR_INVALID_SUBCOMMAND 0x3E
#define CTAP2_ERR_UV_INVALID 0x3F
#define CTAP2_ERR_UNAUTHORIZED_PERMISSION 0x40

/* CTAP2 Constants */
#define CTAP2_MAX_MESSAGE_SIZE 1024
#define CTAP2_MAX_CREDENTIAL_ID_LENGTH 1024
#define CTAP2_MAX_RP_ID_LENGTH 256
#define CTAP2_MAX_USER_ID_LENGTH 64
#define CTAP2_MAX_USER_NAME_LENGTH 64
#define CTAP2_MAX_DISPLAY_NAME_LENGTH 64

/* Public Key Algorithm Identifiers (COSE) */
#define COSE_ALG_ES256 -7  /* ECDSA w/ SHA-256 */
#define COSE_ALG_EDDSA -8  /* EdDSA */
#define COSE_ALG_ES384 -35 /* ECDSA w/ SHA-384 */
#define COSE_ALG_ES512 -36 /* ECDSA w/ SHA-512 */

/* Authenticator Data Flags */
#define CTAP2_AUTH_DATA_FLAG_UP 0x01 /* User Present */
#define CTAP2_AUTH_DATA_FLAG_UV 0x04 /* User Verified */
#define CTAP2_AUTH_DATA_FLAG_BE 0x08 /* Backup Eligible */
#define CTAP2_AUTH_DATA_FLAG_BS 0x10 /* Backup State */
#define CTAP2_AUTH_DATA_FLAG_AT 0x40 /* Attested Credential Data */
#define CTAP2_AUTH_DATA_FLAG_ED 0x80 /* Extension Data */

/* Extension IDs */
#define CTAP2_EXT_HMAC_SECRET "hmac-secret"
#define CTAP2_EXT_CRED_PROTECT "credProtect"
#define CTAP2_EXT_LARGE_BLOBS "largeBlobKey"
#define CTAP2_EXT_MIN_PIN_LENGTH "minPinLength"

/**
 * @brief CTAP2 Request Structure
 */
typedef struct {
    uint8_t cmd;     /**< Command code */
    uint8_t *data;   /**< Request data (CBOR encoded) */
    size_t data_len; /**< Length of request data */
} ctap2_request_t;

/**
 * @brief CTAP2 Response Structure
 */
typedef struct {
    uint8_t status;  /**< Status code */
    uint8_t *data;   /**< Response data (CBOR encoded) */
    size_t data_len; /**< Length of response data */
} ctap2_response_t;

/**
 * @brief Initialize CTAP2 protocol handler
 *
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_init(void);

/**
 * @brief Process a CTAP2 request
 *
 * @param request Pointer to request structure
 * @param response Pointer to response structure
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_process_request(const ctap2_request_t *request, ctap2_response_t *response);

/**
 * @brief Handle MakeCredential command (0x01)
 *
 * @param request_data CBOR-encoded request data
 * @param request_len Length of request data
 * @param response_data Buffer for CBOR-encoded response
 * @param response_len Pointer to response length
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_make_credential(const uint8_t *request_data, size_t request_len,
                              uint8_t *response_data, size_t *response_len);

/**
 * @brief Handle GetAssertion command (0x02)
 *
 * @param request_data CBOR-encoded request data
 * @param request_len Length of request data
 * @param response_data Buffer for CBOR-encoded response
 * @param response_len Pointer to response length
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_get_assertion(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                            size_t *response_len);

/**
 * @brief Handle GetInfo command (0x04)
 *
 * @param response_data Buffer for CBOR-encoded response
 * @param response_len Pointer to response length
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_get_info(uint8_t *response_data, size_t *response_len);

/**
 * @brief Handle ClientPIN command (0x06)
 *
 * @param request_data CBOR-encoded request data
 * @param request_len Length of request data
 * @param response_data Buffer for CBOR-encoded response
 * @param response_len Pointer to response length
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_client_pin(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                         size_t *response_len);

/**
 * @brief Handle Reset command (0x07)
 *
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_reset(void);

/**
 * @brief Handle GetNextAssertion command (0x08)
 *
 * @param response_data Buffer for CBOR-encoded response
 * @param response_len Pointer to response length
 * @return CTAP2_OK on success, error code otherwise
 */
uint8_t ctap2_get_next_assertion(uint8_t *response_data, size_t *response_len);

/**
 * @brief Handle CTAP2 credential management command (0x0A)
 *
 * @param request_data Request data buffer
 * @param request_len Request data length
 * @param response_data Response data buffer
 * @param response_len Pointer to response data length
 * @return CTAP2 status code
 */
uint8_t ctap2_credential_management(const uint8_t *request_data, size_t request_len,
                                    uint8_t *response_data, size_t *response_len);

/**
 * @brief Handle CTAP2 authenticator configuration command (0x0D)
 *
 * @param request_data Request data buffer
 * @param request_len Request data length
 * @param response_data Response data buffer
 * @param response_len Pointer to response data length
 * @return CTAP2 status code
 */
uint8_t ctap2_authenticator_config(const uint8_t *request_data, size_t request_len,
                                    uint8_t *response_data, size_t *response_len);

#ifdef __cplusplus
}
#endif

#endif /* CTAP2_H */
