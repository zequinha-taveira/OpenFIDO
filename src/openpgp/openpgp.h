/**
 * @file openpgp.h
 * @brief OpenPGP Card Implementation
 *
 * Based on OpenPGP Card specification v3.4
 * Compatible with GnuPG and other OpenPGP implementations
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef OPENPGP_H
#define OPENPGP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OpenPGP Application Identifier (AID) */
#define OPENPGP_AID "\xD2\x76\x00\x01\x24\x01"
#define OPENPGP_AID_LEN 6

/* OpenPGP Instructions */
#define OPENPGP_INS_VERIFY              0x20
#define OPENPGP_INS_CHANGE_REFERENCE    0x24
#define OPENPGP_INS_RESET_RETRY         0x2C
#define OPENPGP_INS_PSO                 0x2A  /* Perform Security Operation */
#define OPENPGP_INS_INTERNAL_AUTH       0x88
#define OPENPGP_INS_GENERATE_ASYMMETRIC 0x47
#define OPENPGP_INS_GET_DATA            0xCA
#define OPENPGP_INS_PUT_DATA            0xDA
#define OPENPGP_INS_GET_CHALLENGE       0x84
#define OPENPGP_INS_TERMINATE           0xE6
#define OPENPGP_INS_ACTIVATE            0x44

/* PSO Operations */
#define OPENPGP_PSO_CDS                 0x9E9A  /* Compute Digital Signature */
#define OPENPGP_PSO_DEC                 0x8086  /* Decipher */
#define OPENPGP_PSO_ENC                 0x8680  /* Encipher */

/* PIN References */
#define OPENPGP_PIN_USER                0x81  /* User PIN (PIN1) */
#define OPENPGP_PIN_RESET               0x82  /* Reset Code */
#define OPENPGP_PIN_ADMIN               0x83  /* Admin PIN (PIN3) */

/* Key References */
#define OPENPGP_KEY_SIG                 0x00  /* Signature key (Key 1) */
#define OPENPGP_KEY_DEC                 0x01  /* Decryption key (Key 2) */
#define OPENPGP_KEY_AUT                 0x02  /* Authentication key (Key 3) */

/* Data Object Tags */
#define OPENPGP_DO_AID                  0x004F
#define OPENPGP_DO_NAME                 0x005B
#define OPENPGP_DO_LANG                 0x5F2D
#define OPENPGP_DO_SEX                  0x5F35
#define OPENPGP_DO_URL                  0x5F50
#define OPENPGP_DO_HISTORICAL           0x5F52
#define OPENPGP_DO_CARDHOLDER_DATA      0x0065
#define OPENPGP_DO_APPLICATION_DATA     0x006E
#define OPENPGP_DO_SECURITY_TEMPLATE    0x007A
#define OPENPGP_DO_CARDHOLDER_CERT      0x7F21
#define OPENPGP_DO_FINGERPRINT_SIG      0x00C7
#define OPENPGP_DO_FINGERPRINT_DEC      0x00C8
#define OPENPGP_DO_FINGERPRINT_AUT      0x00C9
#define OPENPGP_DO_CA_FINGERPRINT_1     0x00CA
#define OPENPGP_DO_CA_FINGERPRINT_2     0x00CB
#define OPENPGP_DO_CA_FINGERPRINT_3     0x00CC
#define OPENPGP_DO_KEY_INFO             0x00DE
#define OPENPGP_DO_PW_STATUS            0x00C4
#define OPENPGP_DO_KEY_SIG_ATTR         0x00C1
#define OPENPGP_DO_KEY_DEC_ATTR         0x00C2
#define OPENPGP_DO_KEY_AUT_ATTR         0x00C3

/* Algorithm Attributes */
#define OPENPGP_ALGO_RSA                0x01
#define OPENPGP_ALGO_ECDH               0x12
#define OPENPGP_ALGO_ECDSA              0x13
#define OPENPGP_ALGO_EDDSA              0x16

/* Status Words */
#define OPENPGP_SW_SUCCESS              0x9000
#define OPENPGP_SW_VERIFY_FAIL          0x6300
#define OPENPGP_SW_WRONG_LENGTH         0x6700
#define OPENPGP_SW_SECURITY_STATUS      0x6982
#define OPENPGP_SW_AUTH_BLOCKED         0x6983
#define OPENPGP_SW_CONDITIONS_NOT_SAT   0x6985
#define OPENPGP_SW_INCORRECT_PARAM      0x6A80
#define OPENPGP_SW_FILE_NOT_FOUND       0x6A82
#define OPENPGP_SW_INCORRECT_P1P2       0x6A86
#define OPENPGP_SW_INS_NOT_SUPPORTED    0x6D00
#define OPENPGP_SW_CLA_NOT_SUPPORTED    0x6E00

/* Configuration */
#define OPENPGP_PIN_MIN_LENGTH          6
#define OPENPGP_PIN_MAX_LENGTH          127
#define OPENPGP_PIN_MAX_RETRIES         3
#define OPENPGP_NAME_MAX_LENGTH         39
#define OPENPGP_LANG_MAX_LENGTH         8
#define OPENPGP_URL_MAX_LENGTH          255

/**
 * @brief OpenPGP Key Slot
 */
typedef struct {
    uint8_t key_ref;
    uint8_t algorithm;
    uint16_t key_size;
    bool generated;
    uint8_t fingerprint[20];
    uint8_t creation_time[4];
    uint8_t public_key[512];
    size_t public_key_len;
    uint8_t private_key[512];
    size_t private_key_len;
} openpgp_key_slot_t;

/**
 * @brief OpenPGP State
 */
typedef struct {
    /* PIN state */
    bool pin_user_verified;
    bool pin_admin_verified;
    uint8_t pin_user_retries;
    uint8_t pin_admin_retries;
    uint8_t pin_user[OPENPGP_PIN_MAX_LENGTH];
    uint8_t pin_user_len;
    uint8_t pin_admin[OPENPGP_PIN_MAX_LENGTH];
    uint8_t pin_admin_len;
    uint8_t reset_code[OPENPGP_PIN_MAX_LENGTH];
    uint8_t reset_code_len;

    /* Cardholder data */
    char name[OPENPGP_NAME_MAX_LENGTH + 1];
    char language[OPENPGP_LANG_MAX_LENGTH + 1];
    uint8_t sex;
    char url[OPENPGP_URL_MAX_LENGTH + 1];

    /* Signature counter */
    uint32_t sig_counter;

    /* Terminated flag */
    bool terminated;
} openpgp_state_t;

/**
 * @brief Initialize OpenPGP module
 *
 * @return 0 on success, error code otherwise
 */
int openpgp_init(void);

/**
 * @brief Handle OpenPGP APDU command
 *
 * @param cmd APDU command
 * @param resp APDU response
 * @return 0 on success, error code otherwise
 */
int openpgp_handle_apdu(const void *cmd, void *resp);

/**
 * @brief Verify PIN
 *
 * @param pin_ref PIN reference
 * @param pin PIN value
 * @param pin_len PIN length
 * @return 0 on success, error code otherwise
 */
int openpgp_verify_pin(uint8_t pin_ref, const uint8_t *pin, size_t pin_len);

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
int openpgp_change_pin(uint8_t pin_ref, const uint8_t *old_pin, size_t old_len,
                       const uint8_t *new_pin, size_t new_len);

/**
 * @brief Generate asymmetric key pair
 *
 * @param key_ref Key reference
 * @param algorithm Algorithm identifier
 * @param key_size Key size in bits
 * @param public_key Output buffer for public key
 * @param public_key_len Output: public key length
 * @return 0 on success, error code otherwise
 */
int openpgp_generate_key(uint8_t key_ref, uint8_t algorithm, uint16_t key_size,
                         uint8_t *public_key, size_t *public_key_len);

/**
 * @brief Compute digital signature
 *
 * @param data Data to sign
 * @param data_len Data length
 * @param signature Output buffer for signature
 * @param sig_len Output: signature length
 * @return 0 on success, error code otherwise
 */
int openpgp_sign(const uint8_t *data, size_t data_len,
                 uint8_t *signature, size_t *sig_len);

/**
 * @brief Decipher data
 *
 * @param data Encrypted data
 * @param data_len Data length
 * @param plaintext Output buffer for plaintext
 * @param plain_len Output: plaintext length
 * @return 0 on success, error code otherwise
 */
int openpgp_decipher(const uint8_t *data, size_t data_len,
                     uint8_t *plaintext, size_t *plain_len);

/**
 * @brief Internal authenticate
 *
 * @param challenge Challenge data
 * @param challenge_len Challenge length
 * @param response Output buffer for response
 * @param resp_len Output: response length
 * @return 0 on success, error code otherwise
 */
int openpgp_authenticate(const uint8_t *challenge, size_t challenge_len,
                         uint8_t *response, size_t *resp_len);

/**
 * @brief Get data object
 *
 * @param tag Data object tag
 * @param data Output buffer
 * @param data_len Output: data length
 * @return 0 on success, error code otherwise
 */
int openpgp_get_data(uint16_t tag, uint8_t *data, size_t *data_len);

/**
 * @brief Put data object
 *
 * @param tag Data object tag
 * @param data Data to store
 * @param data_len Data length
 * @return 0 on success, error code otherwise
 */
int openpgp_put_data(uint16_t tag, const uint8_t *data, size_t data_len);

/**
 * @brief Reset OpenPGP application to factory defaults
 *
 * @return 0 on success, error code otherwise
 */
int openpgp_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENPGP_H */
