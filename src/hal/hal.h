/**
 * @file hal.h
 * @brief Hardware Abstraction Layer Interface
 * 
 * This file defines the HAL interface that must be implemented for each
 * supported hardware platform (ESP32, STM32, nRF52, etc.)
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HAL Return Codes */
#define HAL_OK                  0
#define HAL_ERROR               -1
#define HAL_ERROR_TIMEOUT       -2
#define HAL_ERROR_NOT_SUPPORTED -3
#define HAL_ERROR_BUSY          -4

/* LED States */
typedef enum {
    HAL_LED_OFF = 0,
    HAL_LED_ON,
    HAL_LED_BLINK_SLOW,
    HAL_LED_BLINK_FAST
} hal_led_state_t;

/* Button States */
typedef enum {
    HAL_BUTTON_RELEASED = 0,
    HAL_BUTTON_PRESSED
} hal_button_state_t;

/**
 * @brief Initialize the hardware platform
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_init(void);

/**
 * @brief Deinitialize the hardware platform
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_deinit(void);

/* ========== USB HID Functions ========== */

/**
 * @brief Initialize USB HID interface
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_usb_init(void);

/**
 * @brief Send data over USB HID
 * 
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int hal_usb_send(const uint8_t *data, size_t len);

/**
 * @brief Receive data from USB HID
 * 
 * @param data Pointer to receive buffer
 * @param max_len Maximum length to receive
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return Number of bytes received, or negative error code
 */
int hal_usb_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);

/**
 * @brief Check if USB is connected
 * 
 * @return true if connected, false otherwise
 */
bool hal_usb_is_connected(void);

/* ========== Flash Storage Functions ========== */

/**
 * @brief Initialize flash storage
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_flash_init(void);

/**
 * @brief Read data from flash
 * 
 * @param offset Offset in flash memory
 * @param data Pointer to read buffer
 * @param len Length to read
 * @return HAL_OK on success, error code otherwise
 */
int hal_flash_read(uint32_t offset, uint8_t *data, size_t len);

/**
 * @brief Write data to flash
 * 
 * @param offset Offset in flash memory
 * @param data Pointer to data to write
 * @param len Length to write
 * @return HAL_OK on success, error code otherwise
 */
int hal_flash_write(uint32_t offset, const uint8_t *data, size_t len);

/**
 * @brief Erase flash sector
 * 
 * @param offset Offset of sector to erase
 * @return HAL_OK on success, error code otherwise
 */
int hal_flash_erase(uint32_t offset);

/**
 * @brief Get flash size
 * 
 * @return Flash size in bytes
 */
size_t hal_flash_get_size(void);

/* ========== Random Number Generation ========== */

/**
 * @brief Generate random bytes
 * 
 * @param buffer Pointer to output buffer
 * @param len Number of random bytes to generate
 * @return HAL_OK on success, error code otherwise
 */
int hal_random_generate(uint8_t *buffer, size_t len);

/* ========== User Presence Detection ========== */

/**
 * @brief Initialize user presence button
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_button_init(void);

/**
 * @brief Get button state
 * 
 * @return Button state (PRESSED or RELEASED)
 */
hal_button_state_t hal_button_get_state(void);

/**
 * @brief Wait for button press with timeout
 * 
 * @param timeout_ms Timeout in milliseconds (0 = wait forever)
 * @return true if button was pressed, false on timeout
 */
bool hal_button_wait_press(uint32_t timeout_ms);

/* ========== LED Indicator ========== */

/**
 * @brief Initialize LED
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_led_init(void);

/**
 * @brief Set LED state
 * 
 * @param state LED state to set
 * @return HAL_OK on success, error code otherwise
 */
int hal_led_set_state(hal_led_state_t state);

/* ========== Cryptographic Acceleration (Optional) ========== */

/**
 * @brief Check if hardware crypto acceleration is available
 * 
 * @return true if available, false otherwise
 */
bool hal_crypto_is_available(void);

/**
 * @brief Hardware-accelerated SHA-256
 * 
 * @param data Input data
 * @param len Length of input data
 * @param hash Output hash (32 bytes)
 * @return HAL_OK on success, error code otherwise
 */
int hal_crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash);

/**
 * @brief Hardware-accelerated ECDSA sign (P-256)
 * 
 * @param private_key Private key (32 bytes)
 * @param hash Message hash (32 bytes)
 * @param signature Output signature (64 bytes: r||s)
 * @return HAL_OK on success, error code otherwise
 */
int hal_crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash,
                          uint8_t *signature);

/* ========== Time Functions ========== */

/**
 * @brief Get current timestamp in milliseconds
 * 
 * @return Timestamp in milliseconds
 */
uint64_t hal_get_timestamp_ms(void);

/**
 * @brief Delay for specified milliseconds
 * 
 * @param ms Milliseconds to delay
 */
void hal_delay_ms(uint32_t ms);

/* ========== Watchdog Functions ========== */

/**
 * @brief Initialize watchdog timer
 * 
 * @param timeout_ms Watchdog timeout in milliseconds
 * @return HAL_OK on success, error code otherwise
 */
int hal_watchdog_init(uint32_t timeout_ms);

/**
 * @brief Feed the watchdog
 * 
 * @return HAL_OK on success, error code otherwise
 */
int hal_watchdog_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
