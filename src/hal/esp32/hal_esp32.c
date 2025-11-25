/**
 * @file hal_esp32.c
 * @brief ESP32 Hardware Abstraction Layer Implementation
 * 
 * Uses ESP-IDF APIs for hardware access
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "hal.h"
#include "logger.h"

#ifdef ESP_PLATFORM

#include "esp_system.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

/* GPIO Configuration */
#define BUTTON_GPIO     GPIO_NUM_0      /* Boot button */
#define LED_GPIO        GPIO_NUM_2      /* Built-in LED */

/* NVS Configuration */
#define NVS_NAMESPACE   "openfido"

/* USB HID Report Descriptor for FIDO */
static const uint8_t hid_report_descriptor[] = {
    0x06, 0xD0, 0xF1,  /* Usage Page (FIDO Alliance) */
    0x09, 0x01,        /* Usage (U2F HID Authenticator Device) */
    0xA1, 0x01,        /* Collection (Application) */
    0x09, 0x20,        /*   Usage (Input Report Data) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255) */
    0x75, 0x08,        /*   Report Size (8) */
    0x95, 0x40,        /*   Report Count (64) */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute) */
    0x09, 0x21,        /*   Usage (Output Report Data) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255) */
    0x75, 0x08,        /*   Report Size (8) */
    0x95, 0x40,        /*   Report Count (64) */
    0x91, 0x02,        /*   Output (Data, Variable, Absolute) */
    0xC0               /* End Collection */
};

/* Global state */
static struct {
    bool initialized;
    nvs_handle_t nvs_handle;
    hal_led_state_t led_state;
    TaskHandle_t led_task_handle;
} hal_esp32_state = {0};

/* LED blink task */
static void led_blink_task(void *arg)
{
    while (1) {
        switch (hal_esp32_state.led_state) {
            case HAL_LED_OFF:
                gpio_set_level(LED_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case HAL_LED_ON:
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case HAL_LED_BLINK_SLOW:
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(LED_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case HAL_LED_BLINK_FAST:
                gpio_set_level(LED_GPIO, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_GPIO, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

int hal_init(void)
{
    LOG_INFO("Initializing ESP32 HAL");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        LOG_ERROR("NVS init failed: %d", ret);
        return HAL_ERROR;
    }

    /* Open NVS namespace */
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &hal_esp32_state.nvs_handle);
    if (ret != ESP_OK) {
        LOG_ERROR("NVS open failed: %d", ret);
        return HAL_ERROR;
    }

    hal_esp32_state.initialized = true;
    LOG_INFO("ESP32 HAL initialized");

    return HAL_OK;
}

int hal_deinit(void)
{
    if (hal_esp32_state.initialized) {
        nvs_close(hal_esp32_state.nvs_handle);
        hal_esp32_state.initialized = false;
    }
    return HAL_OK;
}

/* ========== USB HID Functions ========== */

int hal_usb_init(void)
{
    LOG_INFO("Initializing USB HID");

    /* Configure TinyUSB */
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        LOG_ERROR("TinyUSB init failed: %d", ret);
        return HAL_ERROR;
    }

    LOG_INFO("USB HID initialized");
    return HAL_OK;
}

int hal_usb_send(const uint8_t *data, size_t len)
{
    if (!hal_esp32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    /* Send via TinyUSB HID */
    if (tud_hid_report(0, data, len)) {
        return len;
    }

    return HAL_ERROR;
}

int hal_usb_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    if (!hal_esp32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    /* Poll for data with timeout */
    uint32_t start = esp_timer_get_time() / 1000;
    
    while (1) {
        if (tud_hid_available()) {
            uint16_t count = tud_hid_n_read(0, data, max_len);
            return count;
        }

        if (timeout_ms > 0) {
            uint32_t elapsed = (esp_timer_get_time() / 1000) - start;
            if (elapsed >= timeout_ms) {
                return HAL_ERROR_TIMEOUT;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return 0;
}

bool hal_usb_is_connected(void)
{
    return tud_mounted();
}

/* ========== Flash Storage Functions ========== */

int hal_flash_init(void)
{
    /* NVS already initialized in hal_init */
    return HAL_OK;
}

int hal_flash_read(uint32_t offset, uint8_t *data, size_t len)
{
    if (!hal_esp32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    char key[16];
    snprintf(key, sizeof(key), "flash_%u", offset);

    size_t required_size = len;
    esp_err_t ret = nvs_get_blob(hal_esp32_state.nvs_handle, key, data, &required_size);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* Return zeros for uninitialized flash */
        memset(data, 0, len);
        return HAL_OK;
    }

    return (ret == ESP_OK) ? HAL_OK : HAL_ERROR;
}

int hal_flash_write(uint32_t offset, const uint8_t *data, size_t len)
{
    if (!hal_esp32_state.initialized || data == NULL) {
        return HAL_ERROR;
    }

    char key[16];
    snprintf(key, sizeof(key), "flash_%u", offset);

    esp_err_t ret = nvs_set_blob(hal_esp32_state.nvs_handle, key, data, len);
    if (ret != ESP_OK) {
        return HAL_ERROR;
    }

    ret = nvs_commit(hal_esp32_state.nvs_handle);
    return (ret == ESP_OK) ? HAL_OK : HAL_ERROR;
}

int hal_flash_erase(uint32_t offset)
{
    if (!hal_esp32_state.initialized) {
        return HAL_ERROR;
    }

    char key[16];
    snprintf(key, sizeof(key), "flash_%u", offset);

    nvs_erase_key(hal_esp32_state.nvs_handle, key);
    nvs_commit(hal_esp32_state.nvs_handle);

    return HAL_OK;
}

size_t hal_flash_get_size(void)
{
    return 64 * 1024;  /* 64KB virtual flash via NVS */
}

/* ========== Random Number Generation ========== */

int hal_random_generate(uint8_t *buffer, size_t len)
{
    if (buffer == NULL || len == 0) {
        return HAL_ERROR;
    }

    esp_fill_random(buffer, len);
    return HAL_OK;
}

/* ========== User Presence Detection ========== */

int hal_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return (gpio_config(&io_conf) == ESP_OK) ? HAL_OK : HAL_ERROR;
}

hal_button_state_t hal_button_get_state(void)
{
    /* Button is active low */
    return (gpio_get_level(BUTTON_GPIO) == 0) ? HAL_BUTTON_PRESSED : HAL_BUTTON_RELEASED;
}

bool hal_button_wait_press(uint32_t timeout_ms)
{
    uint32_t start = esp_timer_get_time() / 1000;

    while (1) {
        if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
            /* Debounce */
            vTaskDelay(pdMS_TO_TICKS(50));
            if (hal_button_get_state() == HAL_BUTTON_PRESSED) {
                return true;
            }
        }

        if (timeout_ms > 0) {
            uint32_t elapsed = (esp_timer_get_time() / 1000) - start;
            if (elapsed >= timeout_ms) {
                return false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ========== LED Indicator ========== */

int hal_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        return HAL_ERROR;
    }

    /* Create LED task */
    xTaskCreate(led_blink_task, "led_task", 2048, NULL, 5, &hal_esp32_state.led_task_handle);

    return HAL_OK;
}

int hal_led_set_state(hal_led_state_t state)
{
    hal_esp32_state.led_state = state;
    return HAL_OK;
}

/* ========== Cryptographic Acceleration ========== */

bool hal_crypto_is_available(void)
{
    /* ESP32 has hardware AES and SHA acceleration */
    return true;
}

int hal_crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    /* ESP32 mbedTLS automatically uses hardware acceleration */
    return HAL_ERROR_NOT_SUPPORTED;
}

int hal_crypto_ecdsa_sign(const uint8_t *private_key, const uint8_t *hash,
                          uint8_t *signature)
{
    /* Not implemented - use software crypto */
    return HAL_ERROR_NOT_SUPPORTED;
}

/* ========== Time Functions ========== */

uint64_t hal_get_timestamp_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void hal_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ========== Watchdog Functions ========== */

int hal_watchdog_init(uint32_t timeout_ms)
{
    /* ESP32 watchdog is automatically enabled by ESP-IDF */
    return HAL_OK;
}

int hal_watchdog_feed(void)
{
    /* Feed task watchdog */
    return HAL_OK;
}

#endif /* ESP_PLATFORM */
