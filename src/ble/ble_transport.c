/**
 * @file ble_transport.c
 * @brief BLE Transport Manager Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ble_transport.h"
#include "ble_fido_service.h"
#include "ble_fragment.h"
#include "../hal/hal_ble.h"
#include "../utils/logger.h"
#include <stdlib.h>
#include <string.h>

/* ========== Transport State ========== */

typedef struct {
    bool initialized;
    ble_transport_state_t state;
    ble_transport_callbacks_t callbacks;
    uint16_t conn_handle;
    uint16_t mtu;
    ble_fragment_buffer_t rx_fragment;
    uint64_t last_activity_ms;
} ble_transport_state_t;

static ble_transport_state_t transport_state = {
    .initialized = false,
    .state = BLE_TRANSPORT_STATE_IDLE,
    .conn_handle = HAL_BLE_CONN_HANDLE_INVALID,
    .mtu = BLE_FRAGMENT_DEFAULT_MTU,
    .last_activity_ms = 0
};

/* ========== Forward Declarations ========== */

static void on_ble_event(const hal_ble_event_t *event);
static void on_control_point_write(uint16_t conn_handle, const uint8_t *data, size_t len);
static void on_status_notify(uint16_t conn_handle, bool enabled);

/* ========== Transport Management ========== */

int ble_transport_init(const ble_transport_callbacks_t *callbacks) {
    if (callbacks == NULL) {
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (transport_state.initialized) {
        LOG_WARN("BLE transport already initialized");
        return BLE_TRANSPORT_OK;
    }

    LOG_INFO("Initializing BLE transport");

    /* Check if BLE is supported */
    if (!hal_ble_is_supported()) {
        LOG_WARN("BLE not supported on this platform");
        return BLE_TRANSPORT_ERROR;
    }

    /* Store callbacks */
    transport_state.callbacks = *callbacks;

    /* Initialize BLE HAL */
    int ret = hal_ble_init(on_ble_event);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("BLE HAL init failed: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    /* Initialize FIDO service */
    ble_fido_service_callbacks_t fido_callbacks = {
        .on_control_point_write = on_control_point_write,
        .on_status_notify = on_status_notify
    };

    ret = ble_fido_service_init(&fido_callbacks);
    if (ret != BLE_FIDO_SERVICE_OK) {
        LOG_ERROR("FIDO service init failed: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    /* Initialize fragment buffer */
    ble_fragment_init(&transport_state.rx_fragment);

    transport_state.initialized = true;
    transport_state.state = BLE_TRANSPORT_STATE_IDLE;

    LOG_INFO("BLE transport initialized");

    return BLE_TRANSPORT_OK;
}

int ble_transport_start(void) {
    if (!transport_state.initialized) {
        LOG_ERROR("BLE transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (transport_state.state != BLE_TRANSPORT_STATE_IDLE) {
        LOG_WARN("BLE transport already started");
        return BLE_TRANSPORT_OK;
    }

    LOG_INFO("Starting BLE transport");

    /* Start advertising */
    int ret = hal_ble_start_advertising(NULL);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to start advertising: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    transport_state.state = BLE_TRANSPORT_STATE_ADVERTISING;

    LOG_INFO("BLE transport started (advertising)");

    return BLE_TRANSPORT_OK;
}

int ble_transport_stop(void) {
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    LOG_INFO("Stopping BLE transport");

    /* Stop advertising */
    hal_ble_stop_advertising();

    /* Disconnect if connected */
    if (transport_state.conn_handle != HAL_BLE_CONN_HANDLE_INVALID) {
        hal_ble_disconnect(transport_state.conn_handle);
        transport_state.conn_handle = HAL_BLE_CONN_HANDLE_INVALID;
    }

    transport_state.state = BLE_TRANSPORT_STATE_IDLE;

    LOG_INFO("BLE transport stopped");

    return BLE_TRANSPORT_OK;
}

ble_transport_state_t ble_transport_get_state(void) {
    return transport_state.state;
}

bool ble_transport_is_connected(void) {
    return transport_state.conn_handle != HAL_BLE_CONN_HANDLE_INVALID &&
           transport_state.state >= BLE_TRANSPORT_STATE_CONNECTED;
}

/* ========== CTAP Message Processing ========== */

int ble_transport_process_request(const uint8_t *data, size_t len) {
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!ble_transport_is_connected()) {
        LOG_ERROR("Cannot process request: not connected");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Processing CTAP request fragment: len=%zu", len);

    /* Add fragment to reassembly buffer */
    int ret = ble_fragment_add(&transport_state.rx_fragment, data, len);
    if (ret != BLE_FRAGMENT_OK) {
        LOG_ERROR("Fragment add failed: %d", ret);
        ble_fragment_reset(&transport_state.rx_fragment);
        return BLE_TRANSPORT_ERROR;
    }

    /* Check if message is complete */
    if (ble_fragment_is_complete(&transport_state.rx_fragment)) {
        uint8_t *msg_data;
        size_t msg_len;

        ret = ble_fragment_get_message(&transport_state.rx_fragment, &msg_data, &msg_len);
        if (ret != BLE_FRAGMENT_OK) {
            LOG_ERROR("Failed to get complete message: %d", ret);
            ble_fragment_reset(&transport_state.rx_fragment);
            return BLE_TRANSPORT_ERROR;
        }

        LOG_INFO("CTAP request complete: %zu bytes", msg_len);

        /* Update state */
        transport_state.state = BLE_TRANSPORT_STATE_PROCESSING;

        /* Invoke callback */
        if (transport_state.callbacks.on_ctap_request != NULL) {
            transport_state.callbacks.on_ctap_request(msg_data, msg_len);
        }

        /* Reset fragment buffer for next message */
        ble_fragment_reset(&transport_state.rx_fragment);

        /* Return to connected state */
        transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;
    }

    return BLE_TRANSPORT_OK;
}

int ble_transport_send_response(const uint8_t *data, size_t len) {
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!ble_transport_is_connected()) {
        LOG_ERROR("Cannot send response: not connected");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_INFO("Sending CTAP response: %zu bytes", len);

    /* Fragment the response */
    uint8_t *fragments[BLE_FRAGMENT_MAX_FRAGMENTS];
    size_t fragment_sizes[BLE_FRAGMENT_MAX_FRAGMENTS];
    size_t num_fragments = 0;

    int ret = ble_fragment_create(data, len, fragments, fragment_sizes, 
                                   &num_fragments, transport_state.mtu);
    if (ret != BLE_FRAGMENT_OK) {
        LOG_ERROR("Failed to create fragments: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    LOG_DEBUG("Created %zu fragments for response", num_fragments);

    /* Send each fragment */
    for (size_t i = 0; i < num_fragments; i++) {
        ret = ble_fido_service_send_status(transport_state.conn_handle, 
                                            fragments[i], 
                                            fragment_sizes[i]);
        if (ret != BLE_FIDO_SERVICE_OK) {
            LOG_ERROR("Failed to send fragment %zu: %d", i, ret);
            
            /* Clean up remaining fragments */
            for (size_t j = i; j < num_fragments; j++) {
                free(fragments[j]);
            }
            
            return BLE_TRANSPORT_ERROR;
        }

        /* Free fragment after sending */
        free(fragments[i]);
    }

    LOG_INFO("CTAP response sent successfully");

    return BLE_TRANSPORT_OK;
}

/* ========== Connection Management ========== */

uint16_t ble_transport_get_connection_handle(void) {
    return transport_state.conn_handle;
}

uint16_t ble_transport_get_mtu(void) {
    return transport_state.mtu;
}

int ble_transport_disconnect(void) {
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (transport_state.conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    int ret = hal_ble_disconnect(transport_state.conn_handle);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to disconnect: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    return BLE_TRANSPORT_OK;
}

/* ========== Event Handlers ========== */

static void on_ble_event(const hal_ble_event_t *event) {
    if (event == NULL) {
        return;
    }

    switch (event->type) {
        case HAL_BLE_EVENT_CONNECTED:
            LOG_INFO("BLE connected: conn_handle=%d", event->conn_handle);
            transport_state.conn_handle = event->conn_handle;
            transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;
            
            /* Get MTU */
            hal_ble_get_mtu(event->conn_handle, &transport_state.mtu);
            LOG_INFO("Connection MTU: %d", transport_state.mtu);
            
            /* Reset fragment buffer */
            ble_fragment_reset(&transport_state.rx_fragment);
            
            /* Notify callback */
            if (transport_state.callbacks.on_connection_change != NULL) {
                transport_state.callbacks.on_connection_change(true);
            }
            break;

        case HAL_BLE_EVENT_DISCONNECTED:
            LOG_INFO("BLE disconnected: conn_handle=%d", event->conn_handle);
            transport_state.conn_handle = HAL_BLE_CONN_HANDLE_INVALID;
            transport_state.state = BLE_TRANSPORT_STATE_ADVERTISING;
            
            /* Reset fragment buffer */
            ble_fragment_reset(&transport_state.rx_fragment);
            
            /* Notify callback */
            if (transport_state.callbacks.on_connection_change != NULL) {
                transport_state.callbacks.on_connection_change(false);
            }
            
            /* Restart advertising */
            hal_ble_start_advertising(NULL);
            break;

        case HAL_BLE_EVENT_WRITE:
            /* Handle Control Point writes */
            if (event->char_handle == ble_fido_service_get_control_point_handle()) {
                ble_fido_service_on_control_point_write(event->conn_handle, 
                                                         event->data, 
                                                         event->data_len);
            }
            break;

        case HAL_BLE_EVENT_NOTIFY_ENABLED:
            LOG_INFO("Notifications enabled on char_handle=%d", event->char_handle);
            if (event->char_handle == ble_fido_service_get_status_handle()) {
                ble_fido_service_set_status_notify_enabled(event->conn_handle, true);
            }
            break;

        case HAL_BLE_EVENT_NOTIFY_DISABLED:
            LOG_INFO("Notifications disabled on char_handle=%d", event->char_handle);
            if (event->char_handle == ble_fido_service_get_status_handle()) {
                ble_fido_service_set_status_notify_enabled(event->conn_handle, false);
            }
            break;

        case HAL_BLE_EVENT_MTU_CHANGED:
            LOG_INFO("MTU changed: %d", event->mtu);
            transport_state.mtu = event->mtu;
            break;

        case HAL_BLE_EVENT_ENCRYPTION_CHANGED:
            LOG_INFO("Encryption changed: encrypted=%d", event->encrypted);
            break;

        case HAL_BLE_EVENT_PAIRING_COMPLETE:
            LOG_INFO("Pairing complete");
            break;

        case HAL_BLE_EVENT_PAIRING_FAILED:
            LOG_ERROR("Pairing failed: error=%d", event->error_code);
            break;

        default:
            break;
    }
}

static void on_control_point_write(uint16_t conn_handle, const uint8_t *data, size_t len) {
    LOG_DEBUG("Control Point write: conn=%d, len=%zu", conn_handle, len);
    
    /* Process the fragment */
    ble_transport_process_request(data, len);
}

static void on_status_notify(uint16_t conn_handle, bool enabled) {
    LOG_INFO("Status notify %s: conn=%d", enabled ? "enabled" : "disabled", conn_handle);
}
