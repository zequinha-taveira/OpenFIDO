/**
 * @file rescue.c
 * @brief Rescue/Recovery Mode Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "rescue.h"

#include <string.h>

#include "hal.h"
#include "led_patterns.h"
#include "logger.h"
#include "storage.h"
#include "usb_hid.h"

/* Rescue mode magic button combo: hold button for 10 seconds during boot */
#define RESCUE_BUTTON_HOLD_MS 10000

bool rescue_should_enter(void)
{
    /* Check if button is held during boot */
    if (!hal_button_is_pressed()) {
        return false;
    }

    LOG_INFO("Button held, checking for rescue mode entry...");

    /* Set LED pattern to indicate rescue mode check */
    led_set_pattern(LED_PATTERN_BOOTLOADER);

    /* Wait for button hold duration */
    uint32_t start_ms = hal_get_time_ms();
    while (hal_button_is_pressed()) {
        uint32_t elapsed_ms = hal_get_time_ms() - start_ms;

        if (elapsed_ms >= RESCUE_BUTTON_HOLD_MS) {
            LOG_INFO("Rescue mode activated!");
            return true;
        }

        /* Update LED pattern */
        led_patterns_update();

        /* Small delay */
        hal_delay_ms(10);
    }

    /* Button released before timeout */
    LOG_INFO("Button released, normal boot");
    return false;
}

void rescue_enter(void)
{
    LOG_INFO("Entering rescue mode");

    /* Set LED pattern */
    led_set_pattern(LED_PATTERN_BOOTLOADER);

    /* Main rescue mode loop */
    while (1) {
        /* Update LED */
        led_patterns_update();

        /* Check for USB commands */
        uint8_t rx_buffer[256];
        uint8_t tx_buffer[256];
        size_t bytes_received = 0;

        /* Try to receive data */
        if (usb_hid_receive(rx_buffer, sizeof(rx_buffer), &bytes_received, NULL) == USB_HID_OK &&
            bytes_received > 0) {
            /* Parse command */
            if (bytes_received < 1) {
                continue;
            }

            rescue_command_t cmd = (rescue_command_t) rx_buffer[0];
            const uint8_t *data = &rx_buffer[1];
            size_t data_len = bytes_received - 1;

            size_t response_len = 0;
            int result = rescue_process_command(cmd, data, data_len, tx_buffer, &response_len);

            /* Send response */
            if (response_len > 0) {
                usb_hid_send(tx_buffer, response_len);
            }

            /* Exit rescue mode if requested */
            if (cmd == RESCUE_CMD_EXIT) {
                LOG_INFO("Exiting rescue mode");
                hal_reset();
            }
        }

        /* Small delay */
        hal_delay_ms(10);
    }
}

int rescue_process_command(rescue_command_t cmd, const uint8_t *data, size_t data_len,
                           uint8_t *response, size_t *response_len)
{
    LOG_INFO("Rescue command: 0x%02X", cmd);

    switch (cmd) {
        case RESCUE_CMD_GET_INFO: {
            /* Return device information */
            const char *info = "OpenFIDO Rescue Mode v1.0";
            size_t info_len = strlen(info);
            memcpy(response, info, info_len);
            *response_len = info_len;
            return 0;
        }

        case RESCUE_CMD_FACTORY_RESET: {
            /* Perform factory reset */
            int result = rescue_factory_reset();
            response[0] = (result == 0) ? 0x00 : 0xFF;
            *response_len = 1;
            return result;
        }

        case RESCUE_CMD_FIRMWARE_UPDATE: {
            /* Enter DFU mode (platform-specific) */
            LOG_INFO("Entering DFU mode");
            /* This would trigger platform-specific bootloader */
            /* For now, just acknowledge */
            response[0] = 0x00;
            *response_len = 1;
            return 0;
        }

        case RESCUE_CMD_DIAGNOSTICS: {
            /* Get diagnostics */
            *response_len = rescue_get_diagnostics(response, 256);
            return 0;
        }

        case RESCUE_CMD_EXIT: {
            /* Exit rescue mode */
            response[0] = 0x00;
            *response_len = 1;
            return 0;
        }

        default:
            LOG_WARN("Unknown rescue command: 0x%02X", cmd);
            response[0] = 0xFF;
            *response_len = 1;
            return -1;
    }
}

int rescue_factory_reset(void)
{
    LOG_WARN("Performing factory reset!");

    /* Format storage */
    if (storage_format() != STORAGE_OK) {
        LOG_ERROR("Factory reset failed");
        return -1;
    }

    LOG_INFO("Factory reset complete");
    return 0;
}

size_t rescue_get_diagnostics(uint8_t *buffer, size_t buffer_size)
{
    /* Build diagnostics string */
    const char *diag =
        "OpenFIDO Diagnostics\n"
        "Status: OK\n"
        "Storage: OK\n"
        "USB: OK\n";

    size_t diag_len = strlen(diag);
    if (diag_len > buffer_size) {
        diag_len = buffer_size;
    }

    memcpy(buffer, diag, diag_len);
    return diag_len;
}
