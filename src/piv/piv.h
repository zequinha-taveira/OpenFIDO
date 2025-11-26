/**
 * @file piv.h
 * @brief PIV (Personal Identity Verification) Implementation
 *
 * Based on NIST SP 800-73-4
 * Compatible with Yubikey PIV functionality
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef PIV_H
#define PIV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PIV Application Identifier (AID) */
#define PIV_AID "\xA0\x00\x00\x03\x08\x00\x00\x10\x00\x01\x00"
#define PIV_AID_LEN 11

/* PIV Instructions */
#define PIV_INS_VERIFY 0x20
#define PIV_INS_CHANGE_REFERENCE 0x24
#define PIV_INS_RESET_RETRY 0x2C
#define PIV_INS_GENERAL_AUTHENTICATE 0x87
#define PIV_INS_PUT_DATA 0xDB
#define PIV_INS_GET_DATA 0xCB
#define PIV_INS_GENERATE_ASYMMETRIC 0x47

/* PIV Key References */
#define PIV_KEY_AUTHENTICATION 0x9A /* PIV Authentication */
#define PIV_KEY_SIGNATURE 0x9C      /* Digital Signature */
#define PIV_KEY_KEY_MANAGEMENT 0x9D /* Key Management */
#define PIV_KEY_CARD_AUTH 0x9E      /* Card Authentication */

/* PIV Data Object Tags */
#define PIV_OBJ_CAPABILITY 0x5FC107     /* Card Capability Container */
#define PIV_OBJ_CHUID 0x5FC102          /* Card Holder Unique ID */
#define PIV_OBJ_AUTHENTICATION 0x5FC105 /* X.509 Cert for PIV Auth */
#define PIV_OBJ_SIGNATURE 0x5FC10A      /* X.509 Cert for Digital Sig */
#define PIV_OBJ_KEY_MANAGEMENT 0x5FC10B /* X.509 Cert for Key Mgmt */
#define PIV_OBJ_CARD_AUTH 0x5FC101      /* X.509 Cert for Card Auth */
#define PIV_OBJ_DISCOVERY 0x7E          /* Discovery Object */
#define PIV_OBJ_KEY_HISTORY 0x5FC10C    /* Key History Object */
#define PIV_OBJ_SECURITY 0x5FC106       /* Security Object */

/* PIV Algorithms */
#define PIV_ALG_RSA_1024 0x06
#define PIV_ALG_RSA_2048 0x07
#define PIV_ALG_ECC_P256 0x11
#define PIV_ALG_ECC_P384 0x14

/* PIV PIN References */
#define PIV_PIN_APP 0x80    /* Application PIN */
#define PIV_PIN_GLOBAL 0x00 /* Global PIN */
#define PIV_PIN_PUK 0x81    /* PIN Unblocking Key */

/* PIV Status Words */
#define PIV_SW_SUCCESS 0x9000
#define PIV_SW_BYTES_REMAINING 0x6100 /* + remaining bytes */
#define PIV_SW_VERIFY_FAIL 0x6300     /* + retries remaining */
#define PIV_SW_WRONG_LENGTH 0x6700
#define PIV_SW_SECURITY_STATUS 0x6982
#define PIV_SW_AUTH_BLOCKED 0x6983
#define PIV_SW_DATA_INVALID 0x6984
#define PIV_SW_CONDITIONS_NOT_SAT 0x6985
#define PIV_SW_INCORRECT_PARAM 0x6A80
#define PIV_SW_FILE_NOT_FOUND 0x6A82
#define PIV_SW_INCORRECT_P1P2 0x6A86
#define PIV_SW_WRONG_DATA 0x6A80
#define PIV_SW_FUNC_NOT_SUPPORTED 0x6A81
#define PIV_SW_INS_NOT_SUPPORTED 0x6D00
#define PIV_SW_CLA_NOT_SUPPORTED 0x6E00

/* PIV Configuration */
#define PIV_MAX_CERT_SIZE 2048
#define PIV_PIN_MAX_LENGTH 8
#define PIV_PIN_MIN_LENGTH 6
#define PIV_PIN_MAX_RETRIES 3
#define PIV_PUK_MAX_RETRIES 3

/**
 * @brief PIV Key Slot
 */
typedef struct {
    uint8_t key_ref;
    uint8_t algorithm;
    bool generated;
    uint8_t public_key[256];
    size_t public_key_len;
    uint8_t private_key[256];
    size_t private_key_len;
} piv_key_slot_t;

/**
 * @brief PIV Certificate Slot
 */
typedef struct {
    uint32_t object_id;
    uint8_t cert[PIV_MAX_CERT_SIZE];
    size_t cert_len;
    bool present;
} piv_cert_slot_t;

/**
 * @brief PIV State
 */
typedef struct {
    bool pin_verified;
    uint8_t pin_retries;
    uint8_t puk_retries;
    uint8_t pin[PIV_PIN_MAX_LENGTH];
    uint8_t pin_len;
    uint8_t puk[PIV_PIN_MAX_LENGTH];
    uint8_t puk_len;
} piv_state_t;

/**
 * @brief Initialize PIV module
 *
 * @return 0 on success, error code otherwise
 */
int piv_init(void);

/**
 * @brief Handle PIV APDU command
 *
 * @param cmd APDU command
 * @param resp APDU response
 * @return 0 on success, error code otherwise
 */
int piv_handle_apdu(const void *cmd, void *resp);

/**
 * @brief Verify PIN
 *
 * @param pin_ref PIN reference (PIV_PIN_APP or PIV_PIN_GLOBAL)
 * @param pin PIN value
 * @param pin_len PIN length
 * @return 0 on success, error code otherwise
 */
int piv_verify_pin(uint8_t pin_ref, const uint8_t *pin, size_t pin_len);

/**
 * @brief Change PIN
 *
 * @param pin_ref PIN reference
 * @param old_pin Old PIN value
 * @param old_len Old PIN length
 * @param new_pin New PIN value
 * @param new_len New PIN length
 * @return 0 on success, error code otherwise
 */
int piv_change_pin(uint8_t pin_ref, const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
                   size_t new_len);

/**
 * @brief Generate asymmetric key pair
 *
 * @param key_ref Key reference (slot)
 * @param algorithm Algorithm identifier
 * @param public_key Output buffer for public key
 * @param public_key_len Output: public key length
 * @return 0 on success, error code otherwise
 */
int piv_generate_key(uint8_t key_ref, uint8_t algorithm, uint8_t *public_key,
                     size_t *public_key_len);

/**
 * @brief Perform general authenticate operation
 *
 * @param key_ref Key reference
 * @param algorithm Algorithm
 * @param data Input data
 * @param data_len Input data length
 * @param response Output buffer
 * @param resp_len Output: response length
 * @return 0 on success, error code otherwise
 */
int piv_general_authenticate(uint8_t key_ref, uint8_t algorithm, const uint8_t *data,
                             size_t data_len, uint8_t *response, size_t *resp_len);

/**
 * @brief Get PIV data object
 *
 * @param object_id Object identifier
 * @param data Output buffer
 * @param data_len Output: data length
 * @return 0 on success, error code otherwise
 */
int piv_get_data(uint32_t object_id, uint8_t *data, size_t *data_len);

/**
 * @brief Put PIV data object
 *
 * @param object_id Object identifier
 * @param data Data to store
 * @param data_len Data length
 * @return 0 on success, error code otherwise
 */
int piv_put_data(uint32_t object_id, const uint8_t *data, size_t data_len);

/**
 * @brief Reset PIV application to factory defaults
 *
 * @return 0 on success, error code otherwise
 */
int piv_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PIV_H */
