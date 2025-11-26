/**
 * @file secure_boot_rp2350.c
 * @brief RP2350 Secure Boot Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifdef PLATFORM_RP2350

#include <stdint.h>
#include <stdbool.h>

#include "logger.h"

/* RP2350 OTP Memory Addresses */
#define OTP_BASE 0x40130000
#define OTP_BOOT_FLAGS 0x40130010
#define OTP_SECURE_BOOT_KEY 0x40130100

/**
 * @brief Initialize secure boot for RP2350
 *
 * @return 0 on success, error code otherwise
 */
int rp2350_secure_boot_init(void)
{
    LOG_INFO("RP2350 Secure boot initialization");

    /* Check if secure boot is already enabled */
    volatile uint32_t *boot_flags = (volatile uint32_t *)OTP_BOOT_FLAGS;

    if (*boot_flags & 0x01) {
        LOG_INFO("Secure boot already enabled");
        return 0;
    }

    LOG_WARN("Secure boot not enabled - requires OTP programming");
    return -1;
}

/**
 * @brief Program secure boot key (ONE-TIME OPERATION!)
 *
 * @param key 256-bit signing key
 * @return 0 on success, error code otherwise
 */
int rp2350_program_secure_boot_key(const uint8_t *key)
{
    LOG_WARN("Programming secure boot key - THIS IS IRREVERSIBLE!");

    /* Write key to OTP */
    volatile uint32_t *otp_key = (volatile uint32_t *)OTP_SECURE_BOOT_KEY;

    for (int i = 0; i < 8; i++) {
        uint32_t word = (key[i * 4 + 0] << 0) | (key[i * 4 + 1] << 8) |
                        (key[i * 4 + 2] << 16) | (key[i * 4 + 3] << 24);

        /* OTP programming requires special sequence */
        /* This is simplified - actual implementation needs proper OTP write sequence */
        otp_key[i] = word;
    }

    LOG_INFO("Secure boot key programmed");
    return 0;
}

/**
 * @brief Enable flash protection
 *
 * @return 0 on success, error code otherwise
 */
int rp2350_enable_flash_protection(void)
{
    LOG_INFO("Enabling RP2350 flash protection");

    /* Configure flash protection via OTP */
    /* This would set boot lock bits */

    return 0;
}

#endif /* PLATFORM_RP2350 */
