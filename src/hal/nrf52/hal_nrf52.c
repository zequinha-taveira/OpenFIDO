/**
 * @file hal_nrf52.c
 * @brief Nordic nRF52 Hardware Abstraction Layer Implementation
 *
 * Uses nRF5 SDK APIs for hardware access
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "hal.h"
#include "logger.h"

#ifdef NRF52

#include "app_timer.h"
#include "app_usbd.h"
#include "app_usbd_hid_generic.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_rng.h"
#include "nrf_drv_usbd.h"
#include "nrf_gpio.h"
#include "nrf_nvmc.h"

/* GPIO Configuration */
#define BUTTON_PIN 13 /* Button 1 on nRF52840 DK */
#define LED_PIN 13    /* LED 1 on nRF52840 DK */

/* Flash Configuration */
#define FLASH_USER_START 0x70000
#define FLASH_USER_SIZE 0x10000 /* 64KB */

/* Global state */
static struct {
    bool initialized;
    hal_led_state_t led_state;
    app_timer_id_t led_timer;
} hal_nrf52_state = {0};

/* LED timer callback */
static void led_timer_handler(void *p_context)
{
    static bool led_on = false;

    switch (hal_nrf52_state.led_state) {
        case HAL_LED_OFF:
            nrf_gpio_pin_clear(LED_PIN);
            break;

        case HAL_LED_ON:
            nrf_gpio_pin_set(LED_PIN);
            break;

        case HAL_LED_BLINK_SLOW:
            led_on = !led_on;
            nrf_gpio_pin_write(LED_PIN, led_on);
            break;

        case HAL_LED_BLINK_FAST:
            led_on = !led_on;
            nrf_gpio_pin_write(LED_PIN, led_on);
            break;
    }
}

int hal_init(void)
{
    LOG_INFO("Initializing nRF52 HAL");

    /* Initialize clock */
    ret_code_t ret = nrf_drv_clock_init();
    if (ret != NRF_SUCCESS && ret != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
        LOG_ERROR("Clock init failed: %d", ret);
        return HAL_ERROR;
    }

    nrf_drv_clock_lfclk_request(NULL);

    /* Initialize app timer */
    ret = app_timer_init();
    if (ret != NRF_SUCCESS) {
        LOG_ERROR("Timer init failed: %d", ret);
        return HAL_ERROR;
    }

    hal_nrf52_state.initialized = true;
    LOG_INFO("nRF52 HAL initialized");

    return HAL_OK;
}

int hal_deinit(void)
{
    hal_nrf52_state.initialized = false;
    return HAL_OK;
}

/* ========== USB HID Functions ========== */

int hal_usb_init(void)
{
    LOG_INFO("Initializing USB HID");

    /* Initialize USB */
    static const app_usbd_config_t usbd_config = {.ev_state_proc = NULL};

    ret_code_t ret = app_usbd_init(&usbd_config);
    if (ret != NRF_SUCCESS) {
        LOG_ERROR("USBD init failed: %d", ret);
        return HAL_ERROR;
    }

    /* Start USB */
    ret = app_usbd_power_events_enable();
    if (ret != NRF_SUCCESS) {
        LOG_ERROR("USB power events failed: %d", ret);
        return HAL_ERROR;
    }

    LOG_INFO("USB HID initialized");
    return HAL_OK;
}

int hal_usb_send(const uint8_t *data, size_t len)
{
    if (!hal_nrf52_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    /* Send via USB HID (implementation depends on app_usbd_hid) */
    /* This is a simplified version */

    return len;
}

int hal_usb_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    if (!hal_nrf52_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    uint32_t start = app_timer_cnt_get();
    uint32_t timeout_ticks = APP_TIMER_TICKS(timeout_ms);

    /* Poll for data */
    while (1) {
        /* Check for USB data (implementation specific) */

        if (timeout_ms > 0) {
            uint32_t elapsed = app_timer_cnt_diff_compute(app_timer_cnt_get(), start);
            if (elapsed >= timeout_ticks) {
                return HAL_ERROR_TIMEOUT;
            }
        }

        nrf_delay_ms(1);
    }

    return 0;
}

bool hal_usb_is_connected(void)
{
    return app_usbd_state_get() == APP_USBD_STATE_Started;
}

/* ========== Flash Storage Functions ========== */

int hal_flash_init(void)
{
    /* Flash is ready after power-on */
    return HAL_OK;
}

int hal_flash_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (data == NULL) {
        return HAL_ERROR;
    }

    uint32_t address = FLASH_USER_START + offset;

    if (offset + len > FLASH_USER_SIZE) {
        return HAL_ERROR;
    }

    memcpy(data, (void *) address, len);
    return HAL_OK;
}

int hal_flash_write(uint32_t offset, const uint8_t *data, size_t len)
{
    if (data == NULL) {
        return HAL_ERROR;
    }

    uint32_t address = FLASH_USER_START + offset;

    if (offset + len > FLASH_USER_SIZE) {
        return HAL_ERROR;
    }

    /* Write data word by word */
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = 0;
        memcpy(&word, &data[i], (len - i >= 4) ? 4 : (len - i));

        nrf_nvmc_write_word(address + i, word);
    }

    return HAL_OK;
}

int hal_flash_erase(uint32_t offset)
{
    uint32_t address = FLASH_USER_START + offset;

    /* Erase page (4KB on nRF52) */
    nrf_nvmc_page_erase(address);

    return HAL_OK;
}

size_t hal_flash_get_size(void)
{
    return FLASH_USER_SIZE;
}

/* ========== Random Number Generation ========== */

int hal_random_generate(uint8_t *buffer, size_t len)
{
    if (buffer == NULL || len == 0) {
        return HAL_ERROR;
    }

    /* Initialize RNG if not already done */
    static bool rng_initialized = false;
    if (!rng_initialized) {
        ret_code_t ret = nrf_drv_rng_init(NULL);
        if (ret != NRF_SUCCESS && ret != NRF_ERROR_MODULE_ALREADY_INITIALIZED) {
            return HAL_ERROR;
        }
        rng_initialized = true;
    }

    /* Generate random bytes */
    uint8_t available;
    size_t offset = 0;

    while (offset < len) {
        nrf_drv_rng_bytes_available(&available);

        if (available > 0) {
            size_t to_read = (len - offset < available) ? (len - offset) : available;
            ret_code_t ret = nrf_drv_rng_rand(&buffer[offset], to_read);
            if (ret != NRF_SUCCESS) {
                return HAL_ERROR;
            }
            offset += to_read;
        }

        nrf_delay_us(100);
    }

    return HAL_OK;
}

/* ========== User Presence Detection ========== */

int hal_button_init(void)
{
    nrf_gpio_cfg_input(BUTTON_PIN, NRF_GPIO_PIN_PULLUP);
    return HAL_OK;
}

hal_button_state_t hal_button_get_state(void)
{
    /* Button is active low */
    return (nrf_gpio_pin_read(BUTTON_PIN) == 0) ? HAL_BUTTON_PRESSED : HAL_BUTTON_RELEASED;
}

bool hal_button_wait_press(uint32_t timeout_ms)
{
    uint32_t start = app_timer_cnt_get();
    uint32_t timeout_ticks = APP_TIMER_TICKS(timeout_ms);

    while (1) {
        if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
            /* Debounce */
            nrf_delay_ms(50);
            if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
                return true;
            }
        }

        if (timeout_ms > 0) {
            uint32_t elapsed = app_timer_cnt_diff_compute(app_timer_cnt_get(), start);
            if (elapsed >= timeout_ticks) {
                return false;
            }
        }

        nrf_delay_ms(10);
    }
}

/* ========== LED Indicator ========== */

int hal_led_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_clear(LED_PIN);

    /* Create LED timer */
    ret_code_t ret =
        app_timer_create(&hal_nrf52_state.led_timer, APP_TIMER_MODE_REPEATED, led_timer_handler);
    if (ret != NRF_SUCCESS) {
        return HAL_ERROR;
    }

    /* Start timer (100ms interval for fast blink) */
    ret = app_timer_start(hal_nrf52_state.led_timer, APP_TIMER_TICKS(100), NULL);
    if (ret != NRF_SUCCESS) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

int hal_led_set_state(hal_led_state_t state)
{
    hal_nrf52_state.led_state = state;
    return HAL_OK;
}

/* ========== Cryptographic Acceleration ========== */

bool hal_crypto_is_available(void)
{
    /* nRF52840 has hardware AES acceleration */
    return true;
}

int hal_crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    /* nRF52 doesn't have hardware SHA-256 */
    return HAL_ERROR_NOT_SUPPORTED;
}

int hal_crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash, uint8_t *signature)
{
    /* Use nRF Crypto library if available */
    return HAL_ERROR_NOT_SUPPORTED;
}

/* ========== Time Functions ========== */

uint64_t hal_get_timestamp_ms(void)
{
    return app_timer_cnt_get() / 32.768; /* Assuming 32.768 kHz clock */
}

void hal_delay_ms(uint32_t ms)
{
    nrf_delay_ms(ms);
}

/* ========== Watchdog Functions ========== */

int hal_watchdog_init(uint32_t timeout_ms)
{
    NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) |
                      (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);

    NRF_WDT->CRV = (32768 * timeout_ms) / 1000; /* 32.768 kHz clock */
    NRF_WDT->RREN = WDT_RREN_RR0_Enabled << WDT_RREN_RR0_Pos;
    NRF_WDT->TASKS_START = 1;

    return HAL_OK;
}

int hal_watchdog_feed(void)
{
    NRF_WDT->RR[0] = WDT_RR_RR_Reload;
    return HAL_OK;
}

#endif /* NRF52 */
