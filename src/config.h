/**
 * @file config.h
 * @brief OpenFIDO Configuration Settings
 *
 * Central configuration for device customization, security, and behavior.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ==========================================================================
 *  USB Configuration
 * ========================================================================== */

/** Vendor ID (VID) - Default: 0xCAFE (Example) */
#define CONFIG_USB_VID              0xCAFE

/** Product ID (PID) - Default: 0xF1D0 (FIDO) */
#define CONFIG_USB_PID              0xF1D0

/** Manufacturer String */
#define CONFIG_USB_MANUFACTURER     "OpenFIDO Community"

/** Product String */
#define CONFIG_USB_PRODUCT          "OpenFIDO Security Key"

/** Serial Number String (Should be unique per device in production) */
#define CONFIG_USB_SERIAL           "OPENFIDO0001"

/* ==========================================================================
 *  LED Configuration
 * ========================================================================== */

/** Slow blink interval in milliseconds (Idle state) */
#define CONFIG_LED_BLINK_SLOW_MS    1000

/** Fast blink interval in milliseconds (Error/Busy state) */
#define CONFIG_LED_BLINK_FAST_MS    100

/** Activity indicator duration in milliseconds */
#define CONFIG_LED_ACTIVITY_MS      50

/* ==========================================================================
 *  Security Configuration
 * ========================================================================== */

/**
 * Allow device reset via CTAP2 reset command.
 * Set to 0 to disable reset capability (permanent credentials).
 */
#define CONFIG_SEC_ALLOW_RESET      1

/**
 * Always require PIN for all operations (even if not strictly required by spec).
 * 1 = Enforce PIN, 0 = Standard behavior
 */
#define CONFIG_SEC_ALWAYS_REQUIRE_PIN 0

/**
 * Minimum PIN length (Standard is 4)
 */
#define CONFIG_SEC_MIN_PIN_LENGTH   4

#endif /* CONFIG_H */
