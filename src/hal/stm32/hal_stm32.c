/**
 * @file hal_stm32.c
 * @brief STM32 Hardware Abstraction Layer Implementation
 * 
 * Uses STM32 HAL APIs for hardware access
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "hal.h"
#include "logger.h"

#ifdef STM32

#include "stm32f4xx_hal.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include <string.h>

/* GPIO Configuration */
#define BUTTON_PIN      GPIO_PIN_0
#define BUTTON_PORT     GPIOA
#define LED_PIN         GPIO_PIN_5
#define LED_PORT        GPIOA

/* Flash Configuration */
#define FLASH_USER_START_ADDR   0x08010000  /* Sector 4 */
#define FLASH_USER_END_ADDR     0x0801FFFF

/* Global state */
static struct {
    bool initialized;
    USBD_HandleTypeDef usb_device;
    RNG_HandleTypeDef hrng;
    hal_led_state_t led_state;
    uint32_t led_last_toggle;
} hal_stm32_state = {0};

int hal_init(void)
{
    LOG_INFO("Initializing STM32 HAL");

    /* Initialize STM32 HAL */
    HAL_Init();

    /* Configure system clock (should be done in SystemClock_Config) */
    
    hal_stm32_state.initialized = true;
    LOG_INFO("STM32 HAL initialized");

    return HAL_OK;
}

int hal_deinit(void)
{
    hal_stm32_state.initialized = false;
    return HAL_OK;
}

/* ========== USB HID Functions ========== */

int hal_usb_init(void)
{
    LOG_INFO("Initializing USB HID");

    /* Initialize USB Device Library */
    USBD_Init(&hal_stm32_state.usb_device, &HID_Desc, 0);

    /* Add HID Class */
    USBD_RegisterClass(&hal_stm32_state.usb_device, &USBD_HID);

    /* Start Device */
    USBD_Start(&hal_stm32_state.usb_device);

    LOG_INFO("USB HID initialized");
    return HAL_OK;
}

int hal_usb_send(const uint8_t *data, size_t len)
{
    if (!hal_stm32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    if (USBD_HID_SendReport(&hal_stm32_state.usb_device, (uint8_t *)data, len) == USBD_OK) {
        return len;
    }

    return HAL_ERROR;
}

int hal_usb_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    if (!hal_stm32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    uint32_t start = HAL_GetTick();

    /* Poll for data */
    while (1) {
        /* Check if data available (implementation depends on USB middleware) */
        /* This is a simplified version */
        
        if (timeout_ms > 0) {
            if ((HAL_GetTick() - start) >= timeout_ms) {
                return HAL_ERROR_TIMEOUT;
            }
        }

        HAL_Delay(1);
    }

    return 0;
}

bool hal_usb_is_connected(void)
{
    return (hal_stm32_state.usb_device.dev_state == USBD_STATE_CONFIGURED);
}

/* ========== Flash Storage Functions ========== */

int hal_flash_init(void)
{
    /* Flash is ready after HAL_Init */
    return HAL_OK;
}

int hal_flash_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (data == NULL) {
        return HAL_ERROR;
    }

    uint32_t address = FLASH_USER_START_ADDR + offset;

    if (address + len > FLASH_USER_END_ADDR) {
        return HAL_ERROR;
    }

    memcpy(data, (void *)address, len);
    return HAL_OK;
}

int hal_flash_write(uint32_t offset, const uint8_t *data, size_t len)
{
    if (data == NULL) {
        return HAL_ERROR;
    }

    uint32_t address = FLASH_USER_START_ADDR + offset;

    if (address + len > FLASH_USER_END_ADDR) {
        return HAL_ERROR;
    }

    /* Unlock flash */
    HAL_FLASH_Unlock();

    /* Write data word by word */
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = 0;
        memcpy(&word, &data[i], (len - i >= 4) ? 4 : (len - i));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    /* Lock flash */
    HAL_FLASH_Lock();

    return HAL_OK;
}

int hal_flash_erase(uint32_t offset)
{
    uint32_t address = FLASH_USER_START_ADDR + offset;

    /* Calculate sector number */
    uint32_t sector = 4;  /* Simplified - should calculate based on address */

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = sector;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sector_error;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK) ? HAL_OK : HAL_ERROR;
}

size_t hal_flash_get_size(void)
{
    return FLASH_USER_END_ADDR - FLASH_USER_START_ADDR + 1;
}

/* ========== Random Number Generation ========== */

int hal_random_generate(uint8_t *buffer, size_t len)
{
    if (buffer == NULL || len == 0) {
        return HAL_ERROR;
    }

    /* Initialize RNG if not already done */
    if (hal_stm32_state.hrng.Instance == NULL) {
        hal_stm32_state.hrng.Instance = RNG;
        if (HAL_RNG_Init(&hal_stm32_state.hrng) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    /* Generate random bytes */
    for (size_t i = 0; i < len; i += 4) {
        uint32_t random;
        if (HAL_RNG_GenerateRandomNumber(&hal_stm32_state.hrng, &random) != HAL_OK) {
            return HAL_ERROR;
        }

        size_t copy_len = (len - i >= 4) ? 4 : (len - i);
        memcpy(&buffer[i], &random, copy_len);
    }

    return HAL_OK;
}

/* ========== User Presence Detection ========== */

int hal_button_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init;
    gpio_init.Pin = BUTTON_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(BUTTON_PORT, &gpio_init);

    return HAL_OK;
}

hal_button_state_t hal_button_get_state(void)
{
    /* Button is active low */
    return (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET) ?
           HAL_BUTTON_PRESSED : HAL_BUTTON_RELEASED;
}

bool hal_button_wait_press(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (1) {
        if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
            /* Debounce */
            HAL_Delay(50);
            if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
                return true;
            }
        }

        if (timeout_ms > 0) {
            if ((HAL_GetTick() - start) >= timeout_ms) {
                return false;
            }
        }

        HAL_Delay(10);
    }
}

/* ========== LED Indicator ========== */

int hal_led_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init;
    gpio_init.Pin = LED_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(LED_PORT, &gpio_init);

    hal_stm32_state.led_state = HAL_LED_OFF;
    hal_stm32_state.led_last_toggle = 0;

    return HAL_OK;
}

int hal_led_set_state(hal_led_state_t state)
{
    hal_stm32_state.led_state = state;

    switch (state) {
        case HAL_LED_OFF:
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
            break;

        case HAL_LED_ON:
            HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
            break;

        default:
            /* Blinking handled in main loop or timer */
            break;
    }

    return HAL_OK;
}

/* ========== Cryptographic Acceleration ========== */

bool hal_crypto_is_available(void)
{
    /* STM32F4 has hardware AES and hash acceleration */
    return true;
}

int hal_crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    /* Use STM32 HASH peripheral if available */
    return HAL_ERROR_NOT_SUPPORTED;
}

int hal_crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash,
                          uint8_t *signature)
{
    return HAL_ERROR_NOT_SUPPORTED;
}

/* ========== Time Functions ========== */

uint64_t hal_get_timestamp_ms(void)
{
    return HAL_GetTick();
}

void hal_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ========== Watchdog Functions ========== */

static IWDG_HandleTypeDef hiwdg;

int hal_watchdog_init(uint32_t timeout_ms)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload = (timeout_ms * 32) / 1000;  /* Simplified calculation */

    return (HAL_IWDG_Init(&hiwdg) == HAL_OK) ? HAL_OK : HAL_ERROR;
}

int hal_watchdog_feed(void)
{
    return (HAL_IWDG_Refresh(&hiwdg) == HAL_OK) ? HAL_OK : HAL_ERROR;
}

#endif /* STM32 */
