/**
 * @file storage.c
 * @brief Secure Credential Storage Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "storage.h"

#include <string.h>

#include "buffer.h"
#include "crypto.h"
#include "hal.h"
#include "logger.h"

/* Storage layout in flash */
#define STORAGE_MAGIC 0x46494432 /* "FID2" */
#define STORAGE_VERSION 1

#define STORAGE_OFFSET_HEADER 0
#define STORAGE_OFFSET_PIN 256
#define STORAGE_OFFSET_COUNTER 512
#define STORAGE_OFFSET_ATT_KEY 768
#define STORAGE_OFFSET_CREDS 2048

#define STORAGE_CRED_SIZE 512

/* Storage header */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t master_key[32]; /* Device master key (encrypted by HAL) */
    uint8_t reserved[216];
} storage_header_t;

/* Encrypted credential on flash */
typedef struct {
    uint8_t id[STORAGE_CREDENTIAL_ID_LENGTH];
    uint8_t encrypted_data[400];
    uint8_t iv[12];
    uint8_t tag[16];
    uint32_t sign_count;
    bool valid;
    uint8_t reserved[59];
} storage_flash_credential_t;

/* Global storage state */
static struct {
    bool initialized;
    storage_header_t header;
    storage_pin_data_t pin_data;
    uint32_t global_counter;
    uint8_t attestation_key[32];
} storage_state = {0};

/* Device master key for credential encryption */
static uint8_t device_master_key[32];

int storage_init(void)
{
    LOG_INFO("Initializing secure storage");

    /* Initialize flash */
    if (hal_flash_init() != HAL_OK) {
        LOG_ERROR("Flash initialization failed");
        return STORAGE_ERROR;
    }

    /* Read header */
    if (hal_flash_read(STORAGE_OFFSET_HEADER, (uint8_t *) &storage_state.header,
                       sizeof(storage_header_t)) != HAL_OK) {
        LOG_ERROR("Failed to read storage header");
        return STORAGE_ERROR;
    }

    /* Check if storage is formatted */
    if (storage_state.header.magic != STORAGE_MAGIC) {
        LOG_WARN("Storage not formatted, formatting now...");
        if (storage_format() != STORAGE_OK) {
            return STORAGE_ERROR;
        }
    }

    /* Derive device master key */
    /* In production, this should use hardware-unique key */
    crypto_random_generate(device_master_key, sizeof(device_master_key));

    /* Read PIN data */
    if (hal_flash_read(STORAGE_OFFSET_PIN, (uint8_t *) &storage_state.pin_data,
                       sizeof(storage_pin_data_t)) != HAL_OK) {
        LOG_ERROR("Failed to read PIN data");
        return STORAGE_ERROR;
    }

    /* Read global counter */
    if (hal_flash_read(STORAGE_OFFSET_COUNTER, (uint8_t *) &storage_state.global_counter,
                       sizeof(uint32_t)) != HAL_OK) {
        LOG_ERROR("Failed to read global counter");
        return STORAGE_ERROR;
    }

    /* Read attestation key */
    if (hal_flash_read(STORAGE_OFFSET_ATT_KEY, storage_state.attestation_key,
                       sizeof(storage_state.attestation_key)) != HAL_OK) {
        LOG_ERROR("Failed to read attestation key");
        return STORAGE_ERROR;
    }

    storage_state.initialized = true;
    LOG_INFO("Storage initialized successfully");

    return STORAGE_OK;
}

int storage_format(void)
{
    LOG_INFO("Formatting storage");

    /* Erase all storage sectors */
    for (uint32_t offset = 0; offset < 64 * 1024; offset += 4096) {
        if (hal_flash_erase(offset) != HAL_OK) {
            LOG_ERROR("Failed to erase flash at offset %u", offset);
            return STORAGE_ERROR;
        }
    }

    /* Initialize header */
    storage_state.header.magic = STORAGE_MAGIC;
    storage_state.header.version = STORAGE_VERSION;
    crypto_random_generate(storage_state.header.master_key, 32);

    /* Write header */
    if (hal_flash_write(STORAGE_OFFSET_HEADER, (const uint8_t *) &storage_state.header,
                        sizeof(storage_header_t)) != HAL_OK) {
        LOG_ERROR("Failed to write storage header");
        return STORAGE_ERROR;
    }

    /* Initialize PIN data */
    memset(&storage_state.pin_data, 0, sizeof(storage_pin_data_t));
    storage_state.pin_data.pin_retries = STORAGE_PIN_MAX_RETRIES;
    storage_state.pin_data.pin_set = false;

    if (hal_flash_write(STORAGE_OFFSET_PIN, (const uint8_t *) &storage_state.pin_data,
                        sizeof(storage_pin_data_t)) != HAL_OK) {
        LOG_ERROR("Failed to write PIN data");
        return STORAGE_ERROR;
    }

    /* Initialize counter */
    storage_state.global_counter = 0;
    if (hal_flash_write(STORAGE_OFFSET_COUNTER, (const uint8_t *) &storage_state.global_counter,
                        sizeof(uint32_t)) != HAL_OK) {
        LOG_ERROR("Failed to write counter");
        return STORAGE_ERROR;
    }

    /* Generate attestation key */
    uint8_t public_key[64];
    crypto_ecdsa_generate_keypair(storage_state.attestation_key, public_key);

    if (hal_flash_write(STORAGE_OFFSET_ATT_KEY, storage_state.attestation_key,
                        sizeof(storage_state.attestation_key)) != HAL_OK) {
        LOG_ERROR("Failed to write attestation key");
        return STORAGE_ERROR;
    }

    LOG_INFO("Storage formatted successfully");
    return STORAGE_OK;
}

int storage_store_credential(const storage_credential_t *credential)
{
    if (!storage_state.initialized || credential == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    /* Find free slot */
    int free_slot = -1;
    for (int i = 0; i < STORAGE_MAX_CREDENTIALS; i++) {
        storage_flash_credential_t flash_cred;
        uint32_t offset = STORAGE_OFFSET_CREDS + (i * STORAGE_CRED_SIZE);

        if (hal_flash_read(offset, (uint8_t *) &flash_cred, sizeof(storage_flash_credential_t)) !=
            HAL_OK) {
            continue;
        }

        if (!flash_cred.valid) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        LOG_ERROR("No free credential slots");
        return STORAGE_ERROR_FULL;
    }

    /* Prepare flash credential */
    storage_flash_credential_t flash_cred = {0};
    memcpy(flash_cred.id, credential->id, STORAGE_CREDENTIAL_ID_LENGTH);
    flash_cred.sign_count = credential->sign_count;
    flash_cred.valid = true;

    /* Generate random IV */
    crypto_random_generate(flash_cred.iv, sizeof(flash_cred.iv));

    /* Serialize credential data */
    uint8_t plaintext[400];
    size_t offset = 0;

    memcpy(&plaintext[offset], credential->rp_id_hash, 32);
    offset += 32;

    memcpy(&plaintext[offset], credential->user_id, credential->user_id_len);
    offset += credential->user_id_len;

    plaintext[offset++] = credential->user_id_len;

    memcpy(&plaintext[offset], credential->private_key, 32);
    offset += 32;

    plaintext[offset++] = credential->resident ? 1 : 0;

    if (credential->resident) {
        strncpy((char *) &plaintext[offset], credential->user_name, STORAGE_MAX_USER_NAME_LENGTH);
        offset += STORAGE_MAX_USER_NAME_LENGTH;

        strncpy((char *) &plaintext[offset], credential->display_name,
                STORAGE_MAX_DISPLAY_NAME_LENGTH);
        offset += STORAGE_MAX_DISPLAY_NAME_LENGTH;

        strncpy((char *) &plaintext[offset], credential->rp_id, STORAGE_MAX_RP_ID_LENGTH);
        offset += STORAGE_MAX_RP_ID_LENGTH;
    }

    /* Encrypt credential */
    if (crypto_aes_gcm_encrypt(device_master_key, flash_cred.iv, credential->rp_id_hash, 32,
                               plaintext, offset, flash_cred.encrypted_data,
                               flash_cred.tag) != CRYPTO_OK) {
        LOG_ERROR("Failed to encrypt credential");
        return STORAGE_ERROR;
    }

    /* Write to flash */
    uint32_t flash_offset = STORAGE_OFFSET_CREDS + (free_slot * STORAGE_CRED_SIZE);
    if (hal_flash_write(flash_offset, (const uint8_t *) &flash_cred,
                        sizeof(storage_flash_credential_t)) != HAL_OK) {
        LOG_ERROR("Failed to write credential to flash");
        return STORAGE_ERROR;
    }

    /* Clean up sensitive data */
    secure_zero(plaintext, sizeof(plaintext));

    LOG_INFO("Stored credential in slot %d", free_slot);
    return STORAGE_OK;
}

int storage_find_credential(const uint8_t *credential_id, storage_credential_t *credential)
{
    if (!storage_state.initialized || credential_id == NULL || credential == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    /* Search all slots */
    for (int i = 0; i < STORAGE_MAX_CREDENTIALS; i++) {
        storage_flash_credential_t flash_cred;
        uint32_t offset = STORAGE_OFFSET_CREDS + (i * STORAGE_CRED_SIZE);

        if (hal_flash_read(offset, (uint8_t *) &flash_cred, sizeof(storage_flash_credential_t)) !=
            HAL_OK) {
            continue;
        }

        if (!flash_cred.valid) {
            continue;
        }

        /* Check if ID matches */
        if (memcmp(flash_cred.id, credential_id, STORAGE_CREDENTIAL_ID_LENGTH) == 0) {
            /* Found it - decrypt */
            uint8_t plaintext[400];

            if (crypto_aes_gcm_decrypt(device_master_key, flash_cred.iv, NULL,
                                       0, /* AAD will be in credential */
                                       flash_cred.encrypted_data, sizeof(flash_cred.encrypted_data),
                                       flash_cred.tag, plaintext) != CRYPTO_OK) {
                LOG_ERROR("Failed to decrypt credential");
                return STORAGE_ERROR_CORRUPTED;
            }

            /* Deserialize */
            size_t off = 0;
            memcpy(credential->rp_id_hash, &plaintext[off], 32);
            off += 32;

            /* Read user_id_len first */
            credential->user_id_len = plaintext[off + STORAGE_MAX_USER_ID_LENGTH];

            memcpy(credential->user_id, &plaintext[off], credential->user_id_len);
            off += credential->user_id_len + 1;

            memcpy(credential->private_key, &plaintext[off], 32);
            off += 32;

            credential->resident = (plaintext[off++] == 1);

            if (credential->resident) {
                strncpy(credential->user_name, (const char *) &plaintext[off],
                        STORAGE_MAX_USER_NAME_LENGTH);
                off += STORAGE_MAX_USER_NAME_LENGTH;

                strncpy(credential->display_name, (const char *) &plaintext[off],
                        STORAGE_MAX_DISPLAY_NAME_LENGTH);
                off += STORAGE_MAX_DISPLAY_NAME_LENGTH;

                strncpy(credential->rp_id, (const char *) &plaintext[off],
                        STORAGE_MAX_RP_ID_LENGTH);
            }

            memcpy(credential->id, flash_cred.id, STORAGE_CREDENTIAL_ID_LENGTH);
            credential->sign_count = flash_cred.sign_count;

            /* Clean up */
            secure_zero(plaintext, sizeof(plaintext));

            LOG_INFO("Found credential in slot %d", i);
            return STORAGE_OK;
        }
    }

    return STORAGE_ERROR_NOT_FOUND;
}

int storage_update_sign_count(const uint8_t *credential_id, uint32_t new_count)
{
    if (!storage_state.initialized || credential_id == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    /* Find credential */
    for (int i = 0; i < STORAGE_MAX_CREDENTIALS; i++) {
        storage_flash_credential_t flash_cred;
        uint32_t offset = STORAGE_OFFSET_CREDS + (i * STORAGE_CRED_SIZE);

        if (hal_flash_read(offset, (uint8_t *) &flash_cred, sizeof(storage_flash_credential_t)) !=
            HAL_OK) {
            continue;
        }

        if (!flash_cred.valid) {
            continue;
        }

        if (memcmp(flash_cred.id, credential_id, STORAGE_CREDENTIAL_ID_LENGTH) == 0) {
            /* Update counter */
            flash_cred.sign_count = new_count;

            if (hal_flash_write(offset, (const uint8_t *) &flash_cred,
                                sizeof(storage_flash_credential_t)) != HAL_OK) {
                LOG_ERROR("Failed to update sign count");
                return STORAGE_ERROR;
            }

            return STORAGE_OK;
        }
    }

    return STORAGE_ERROR_NOT_FOUND;
}

int storage_get_and_increment_counter(uint32_t *counter)
{
    if (!storage_state.initialized || counter == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    *counter = storage_state.global_counter++;

    /* Write back to flash */
    if (hal_flash_write(STORAGE_OFFSET_COUNTER, (const uint8_t *) &storage_state.global_counter,
                        sizeof(uint32_t)) != HAL_OK) {
        LOG_ERROR("Failed to write counter");
        return STORAGE_ERROR;
    }

    return STORAGE_OK;
}

int storage_set_pin(const uint8_t *pin, size_t pin_len)
{
    if (!storage_state.initialized || pin == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    if (pin_len < STORAGE_PIN_MIN_LENGTH || pin_len > STORAGE_PIN_MAX_LENGTH) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    /* Hash PIN */
    crypto_sha256(pin, pin_len, storage_state.pin_data.pin_hash);

    storage_state.pin_data.pin_set = true;
    storage_state.pin_data.pin_retries = STORAGE_PIN_MAX_RETRIES;

    /* Write to flash */
    if (hal_flash_write(STORAGE_OFFSET_PIN, (const uint8_t *) &storage_state.pin_data,
                        sizeof(storage_pin_data_t)) != HAL_OK) {
        LOG_ERROR("Failed to write PIN data");
        return STORAGE_ERROR;
    }

    LOG_INFO("PIN set successfully");
    return STORAGE_OK;
}

int storage_verify_pin(const uint8_t *pin, size_t pin_len)
{
    if (!storage_state.initialized || pin == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    if (!storage_state.pin_data.pin_set) {
        return STORAGE_ERROR;
    }

    if (storage_state.pin_data.pin_retries == 0) {
        return STORAGE_ERROR;
    }

    /* Hash provided PIN */
    uint8_t pin_hash[32];
    crypto_sha256(pin, pin_len, pin_hash);

    /* Constant-time comparison */
    if (constant_time_compare(pin_hash, storage_state.pin_data.pin_hash, 32) == 0) {
        /* PIN correct */
        storage_state.pin_data.pin_retries = STORAGE_PIN_MAX_RETRIES;

        hal_flash_write(STORAGE_OFFSET_PIN, (const uint8_t *) &storage_state.pin_data,
                        sizeof(storage_pin_data_t));

        return STORAGE_OK;
    } else {
        /* PIN incorrect */
        storage_state.pin_data.pin_retries--;

        hal_flash_write(STORAGE_OFFSET_PIN, (const uint8_t *) &storage_state.pin_data,
                        sizeof(storage_pin_data_t));

        LOG_WARN("PIN verification failed, %d retries remaining",
                 storage_state.pin_data.pin_retries);

        return STORAGE_ERROR;
    }
}

bool storage_is_pin_set(void)
{
    return storage_state.initialized && storage_state.pin_data.pin_set;
}

int storage_get_pin_retries(void)
{
    if (!storage_state.initialized) {
        return -1;
    }
    return storage_state.pin_data.pin_retries;
}

bool storage_is_pin_blocked(void)
{
    return storage_state.initialized && (storage_state.pin_data.pin_retries == 0);
}

int storage_get_attestation_key(uint8_t *private_key)
{
    if (!storage_state.initialized || private_key == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    memcpy(private_key, storage_state.attestation_key, 32);
    return STORAGE_OK;
}

int storage_set_attestation_key(const uint8_t *private_key)
{
    if (!storage_state.initialized || private_key == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    memcpy(storage_state.attestation_key, private_key, 32);

    if (hal_flash_write(STORAGE_OFFSET_ATT_KEY, storage_state.attestation_key,
                        sizeof(storage_state.attestation_key)) != HAL_OK) {
        LOG_ERROR("Failed to write attestation key");
        return STORAGE_ERROR;
    }

    return STORAGE_OK;
}

int storage_get_credential_count(size_t *count)
{
    if (!storage_state.initialized || count == NULL) {
        return STORAGE_ERROR_INVALID_PARAM;
    }

    *count = 0;

    for (int i = 0; i < STORAGE_MAX_CREDENTIALS; i++) {
        storage_flash_credential_t flash_cred;
        uint32_t offset = STORAGE_OFFSET_CREDS + (i * STORAGE_CRED_SIZE);

        if (hal_flash_read(offset, (uint8_t *) &flash_cred, sizeof(storage_flash_credential_t)) !=
            HAL_OK) {
            continue;
        }

        if (flash_cred.valid) {
            (*count)++;
        }
    }

    return STORAGE_OK;
}
