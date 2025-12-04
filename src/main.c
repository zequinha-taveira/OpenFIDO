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
#include "hal.h"
#include "logger.h"
#include "openpgp.h"
#include "piv.h"
#include "storage.h"
#include "transport/transport.h"
#include "u2f.h"
#include "usb_ccid.h"
#include "usb_hid.h"
#include "ykman.h"

/* Include BLE transport if supported */
#include "hal/hal_ble.h"
#include "ble/ble_transport.h"

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

    /* Initialize transport abstraction layer */
    LOG_INFO("Initializing transport abstraction layer...");
    ret = transport_init();
    if (ret != TRANSPORT_OK) {
        LOG_ERROR("Transport abstraction initialization failed: %d", ret);
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

    /* Register USB HID with transport abstraction */
    ret = usb_hid_register_transport();
    if (ret != 0) {
        LOG_ERROR("USB HID transport registration failed: %d", ret);
        return -1;
    }

    /* Initialize BLE transport if supported */
    if (hal_ble_is_supported()) {
        LOG_INFO("Initializing BLE transport...");
        
        /* BLE callbacks will be set up later in main loop initialization */
        /* For now, we just check if it's supported */
        LOG_INFO("BLE is supported on this platform");
    } else {
        LOG_INFO("BLE not supported on this platform, continuing with USB only");
    }

    /* Initialize CCID (Smart Card) */
    LOG_INFO("Initializing CCID interface...");
    ret = usb_ccid_init();
    if (ret != 0) {
        LOG_ERROR("CCID initialization failed: %d", ret);
        return -1;
    }

    /* Initialize CTAP2 protocol handler */
    LOG_INFO("Initializing CTAP2 protocol...");
    ret = ctap2_init();
    if (ret != CTAP2_OK) {
        LOG_ERROR("CTAP2 initialization failed: %d", ret);
        return -1;
    }

    /* Initialize PIV (Personal Identity Verification) */
    LOG_INFO("Initializing PIV module...");
    ret = piv_init();
    if (ret != 0) {
        LOG_ERROR("PIV initialization failed: %d", ret);
        return -1;
    }

    /* Initialize OpenPGP Card */
    LOG_INFO("Initializing OpenPGP module...");
    ret = openpgp_init();
    if (ret != 0) {
        LOG_ERROR("OpenPGP initialization failed: %d", ret);
        return -1;
    }

    /* Initialize Yubico Management */
    LOG_INFO("Initializing Yubico Management...");
    ret = ykman_init();
    if (ret != 0) {
        LOG_ERROR("Yubico Management initialization failed: %d", ret);
        return -1;
    }

    LOG_INFO("All subsystems initialized successfully");
    return 0;
}

/**
 * @brief BLE CTAP request callback
 *
 * Called when a complete CTAP request is received over BLE.
 * Processes the request and sends the response back over BLE.
 */
static void on_ble_ctap_request(const uint8_t *data, size_t len)
{
    static uint8_t tx_buffer[CTAP2_MAX_MESSAGE_SIZE];
    ctap2_request_t request;
    ctap2_response_t response;

    LOG_DEBUG("BLE CTAP request received: %zu bytes", len);

    if (len < 1) {
        LOG_ERROR("Invalid CTAP request: too short");
        return;
    }

    /* Parse CTAP2 request */
    request.cmd = data[0];
    request.data = (len > 1) ? &data[1] : NULL;
    request.data_len = (len > 1) ? (len - 1) : 0;

    /* Prepare response buffer */
    response.data = tx_buffer + 1; /* Reserve first byte for status */
    response.data_len = 0;

    /* Process CTAP2 request */
    response.status = ctap2_process_request(&request, &response);

    /* Send response */
    tx_buffer[0] = response.status;
    int total_len = 1 + response.data_len;

    /* Response is sent via transport abstraction (already set to BLE) */
    int ret = transport_send(tx_buffer, total_len);
    if (ret < 0) {
        LOG_ERROR("Failed to send BLE response: %d", ret);
    } else {
        LOG_DEBUG("Sent %d bytes BLE response (status: 0x%02X)", total_len, response.status);
    }
}

/**
 * @brief BLE connection state change callback
 *
 * Called when BLE connection state changes.
 */
static void on_ble_connection_change(bool connected)
{
    if (connected) {
        LOG_INFO("BLE client connected");
        hal_led_set_state(HAL_LED_ON);
    } else {
        LOG_INFO("BLE client disconnected");
        hal_led_set_state(HAL_LED_BLINK_SLOW);
    }
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

    /* Initialize and start BLE transport if supported */
    if (hal_ble_is_supported()) {
        ble_transport_callbacks_t ble_callbacks = {
            .on_ctap_request = on_ble_ctap_request,
            .on_connection_change = on_ble_connection_change
        };

        int ret = ble_transport_init(&ble_callbacks);
        if (ret == BLE_TRANSPORT_OK) {
            /* Register BLE with transport abstraction */
            ret = ble_transport_register();
            if (ret == BLE_TRANSPORT_OK) {
                /* Start BLE advertising */
                ret = ble_transport_start();
                if (ret == BLE_TRANSPORT_OK) {
                    LOG_INFO("BLE transport started successfully");
                } else {
                    LOG_ERROR("Failed to start BLE transport: %d", ret);
                }
            } else {
                LOG_ERROR("Failed to register BLE transport: %d", ret);
            }
        } else {
            LOG_ERROR("Failed to initialize BLE transport: %d", ret);
        }
    }

    while (1) {
        /* Feed watchdog */
        hal_watchdog_feed();

        /* Poll USB transport for data */
        uint8_t cmd = 0;
        bytes_received = transport_receive_from(TRANSPORT_TYPE_USB, rx_buffer, sizeof(rx_buffer), &cmd);

        if (bytes_received > 0) {
            LOG_DEBUG("Received %d bytes from USB (CMD: 0x%02X)", bytes_received, cmd);

            /* Check if another operation is in progress */
            if (transport_is_busy()) {
                LOG_WARN("Operation already in progress, rejecting request");
                /* In a real implementation, we would send a CTAPHID_ERROR response */
                continue;
            }

            /* Set USB as active transport and lock it for this operation */
            transport_set_active(TRANSPORT_TYPE_USB);
            transport_lock(TRANSPORT_TYPE_USB);
            transport_set_busy(true);

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

                /* Send response on active transport */
                tx_buffer[0] = response.status;
                int total_len = 1 + response.data_len;

                transport_send(tx_buffer, total_len);
                LOG_DEBUG("Sent %d bytes response (status: 0x%02X)", total_len, response.status);
            } else if (cmd == CTAPHID_MSG) {
                /* Process U2F APDU */
                size_t response_len = 0;
                uint16_t sw = u2f_process_apdu(rx_buffer, bytes_received, tx_buffer, &response_len);

                /* Append SW to response */
                tx_buffer[response_len++] = (sw >> 8) & 0xFF;
                tx_buffer[response_len++] = sw & 0xFF;

                transport_send(tx_buffer, response_len);
                LOG_DEBUG("Sent %zu bytes U2F response (SW: 0x%04X)", response_len, sw);
            } else {
                LOG_WARN("Unknown or unsupported CTAPHID command: 0x%02X", cmd);
                /* Send error response? Or just ignore? CTAPHID spec says send ERROR */
                uint8_t err_payload[1] = {0x01}; /* INVALID_CMD */
                /* We can't easily send CTAPHID ERROR from here without exposing more usb_hid
                 * internals */
                /* For now, just ignore */
            }

            /* Clear operation state */
            transport_set_busy(false);
            transport_unlock();

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
