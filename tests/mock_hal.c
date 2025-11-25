/**
 * @file mock_hal.c
 * @brief Mock HAL implementation for unit testing
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "hal.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Mock flash storage */
static uint8_t mock_flash[64 * 1024];
static bool mock_button_pressed = false;
static bool mock_initialized = false;

int hal_init(void)
{
    memset(mock_flash, 0xFF, sizeof(mock_flash));
    mock_initialized = true;
    return HAL_OK;
}

int hal_deinit(void)
{
    mock_initialized = false;
    return HAL_OK;
}

int hal_usb_init(void)
{
    return HAL_OK;
}

int hal_usb_send(const uint8_t *data, size_t len)
{
    return len;
}

int hal_usb_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    return 0;
}

bool hal_usb_is_connected(void)
{
    return true;
}

int hal_flash_init(void)
{
    return HAL_OK;
}

int hal_flash_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (offset + len > sizeof(mock_flash)) {
        return HAL_ERROR;
    }
    memcpy(data, &mock_flash[offset], len);
    return HAL_OK;
}

int hal_flash_write(uint32_t offset, const uint8_t *data, size_t len)
{
    if (offset + len > sizeof(mock_flash)) {
        return HAL_ERROR;
    }
    memcpy(&mock_flash[offset], data, len);
    return HAL_OK;
}

int hal_flash_erase(uint32_t offset)
{
    if (offset >= sizeof(mock_flash)) {
        return HAL_ERROR;
    }
    memset(&mock_flash[offset], 0xFF, 4096);
    return HAL_OK;
}

size_t hal_flash_get_size(void)
{
    return sizeof(mock_flash);
}

int hal_random_generate(uint8_t *buffer, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        buffer[i] = rand() & 0xFF;
    }
    return HAL_OK;
}

int hal_button_init(void)
{
    return HAL_OK;
}

hal_button_state_t hal_button_get_state(void)
{
    return mock_button_pressed ? HAL_BUTTON_PRESSED : HAL_BUTTON_RELEASED;
}

bool hal_button_wait_press(uint32_t timeout_ms)
{
    /* Auto-press for testing */
    return true;
}

void mock_set_button_pressed(bool pressed)
{
    mock_button_pressed = pressed;
}

int hal_led_init(void)
{
    return HAL_OK;
}

int hal_led_set_state(hal_led_state_t state)
{
    return HAL_OK;
}

bool hal_crypto_is_available(void)
{
    return false;
}

int hal_crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    return HAL_ERROR_NOT_SUPPORTED;
}

int hal_crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash,
                          uint8_t *signature)
{
    return HAL_ERROR_NOT_SUPPORTED;
}

uint64_t hal_get_timestamp_ms(void)
{
    return (uint64_t)time(NULL) * 1000;
}

void hal_delay_ms(uint32_t ms)
{
    /* No delay in tests */
}

int hal_watchdog_init(uint32_t timeout_ms)
{
    return HAL_OK;
}

int hal_watchdog_feed(void)
{
    return HAL_OK;
}
