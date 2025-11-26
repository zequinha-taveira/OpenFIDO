/**
 * @file main.c
 * @brief OpenFIDO Main Application Entry Point
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "crypto.h"
#include "ctap2.h"
#include "u2f.h"
#include "hal.h"
#include "logger.h"
#include "storage.h"
#include "usb_hid.h"

#define APP_VERSION "1.0.0"

/**
 * @brief Initialize all subsystems
 *
 * @return 0 on success, negative error code otherwise
 */
static int init_subsystems(void)
{
    int ret;

    /* Initialize logger */
    logger_init();
    LOG_INFO("OpenFIDO v%s starting...", APP_VERSION);
    LOG_INFO("Device: %s %s", CONFIG_USB_MANUFACTURER, CONFIG_USB_PRODUCT);

    /* Initialize HAL */
    LOG_INFO("Initializing hardware abstraction layer...");
    ret = hal_init();
    if (ret != HAL_OK) {
        LOG_ERROR("HAL initialization failed: %d", ret);
        return -1;
    }

    /* Initialize cryptographic library */
    LOG_INFO("Initializing cryptographic library...");
    ret = crypto_init();
    if (ret != CRYPTO_OK) {
        LOG_ERROR("Crypto initialization failed: %d", ret);
        return -1;
    }

    /* Initialize storage */
    LOG_INFO("Initializing secure storage...");
    ret = storage_init();
    if (ret != STORAGE_OK) {
        LOG_ERROR("Storage initialization failed: %d", ret);
        return -1;
    }

    /* Initialize USB HID */
    LOG_INFO("Initializing USB HID interface...");
    ret = usb_hid_init();
    if (ret != 0) {
        LOG_ERROR("USB HID initialization failed: %d", ret);
        return -1;
    }

    /* Initialize CTAP2 protocol handler */
    LOG_INFO("Initializing CTAP2 protocol...");
    ret = ctap2_init();
    if (ret != CTAP2_OK) {
        LOG_ERROR("CTAP2 initialization failed: %d", ret);
        return -1;
    }

    LOG_INFO("All subsystems initialized successfully");
    return 0;
}

/**
 * @brief Main application loop
 */
static void main_loop(void)
{
    uint8_t rx_buffer[CTAP2_MAX_MESSAGE_SIZE];
    uint8_t tx_buffer[CTAP2_MAX_MESSAGE_SIZE];
    ctap2_request_t request;
    ctap2_response_t response;
    int bytes_received;

    LOG_INFO("Entering main loop...");
    hal_led_set_state(HAL_LED_BLINK_SLOW);

    while (1) {
        /* Feed watchdog */
        hal_watchdog_feed();

        /* Wait for USB data */
        uint8_t cmd = 0;
        bytes_received = usb_hid_receive(rx_buffer, sizeof(rx_buffer), &cmd);

        if (bytes_received > 0) {
            LOG_DEBUG("Received %d bytes from USB (CMD: 0x%02X)", bytes_received, cmd);

            /* Indicate activity */
            hal_led_set_state(HAL_LED_ON);

            if (cmd == CTAPHID_CBOR) {
                /* Parse CTAP2 request */
                request.cmd = rx_buffer[0];
                request.data = &rx_buffer[1];
                request.data_len = bytes_received - 1;

                /* Prepare response buffer */
                response.data = tx_buffer + 1; /* Reserve first byte for status */
                response.data_len = 0;

                /* Process CTAP2 request */
                response.status = ctap2_process_request(&request, &response);

                /* Send response */
                tx_buffer[0] = response.status;
                int total_len = 1 + response.data_len;

                usb_hid_send(tx_buffer, total_len);
                LOG_DEBUG("Sent %d bytes response (status: 0x%02X)", total_len, response.status);
            } else if (cmd == CTAPHID_MSG) {
                /* Process U2F APDU */
                size_t response_len = 0;
                uint16_t sw = u2f_process_apdu(rx_buffer, bytes_received, tx_buffer, &response_len);
                
                /* Append SW to response */
                tx_buffer[response_len++] = (sw >> 8) & 0xFF;
                tx_buffer[response_len++] = sw & 0xFF;

                usb_hid_send(tx_buffer, response_len);
                LOG_DEBUG("Sent %zu bytes U2F response (SW: 0x%04X)", response_len, sw);
            } else {
                LOG_WARN("Unknown or unsupported CTAPHID command: 0x%02X", cmd);
                /* Send error response? Or just ignore? CTAPHID spec says send ERROR */
                uint8_t err_payload[1] = {0x01}; /* INVALID_CMD */
                /* We can't easily send CTAPHID ERROR from here without exposing more usb_hid internals */
                /* For now, just ignore */
            }

            /* Return to idle state */
            /* Note: In a real implementation with non-blocking LED, we would use a timer here
               to keep the LED on for CONFIG_LED_ACTIVITY_MS before returning to slow blink.
               For now, we just switch back. */
            hal_led_set_state(HAL_LED_BLINK_SLOW);
        }

        /* Small delay to prevent busy-waiting */
        hal_delay_ms(10);
    }
}

/**
 * @brief Main entry point
 */
int main(void)
{
    /* Initialize all subsystems */
    if (init_subsystems() != 0) {
        /* Initialization failed - indicate error */
        hal_led_set_state(HAL_LED_BLINK_FAST);
        while (1) {
            hal_delay_ms(1000);
        }
    }

    /* Indicate ready */
    hal_led_set_state(HAL_LED_ON);
    hal_delay_ms(CONFIG_LED_ACTIVITY_MS * 10);
    hal_led_set_state(HAL_LED_OFF);
    hal_delay_ms(CONFIG_LED_ACTIVITY_MS * 10);

    /* Start main loop */
    main_loop();

    /* Should never reach here */
    return 0;
}
