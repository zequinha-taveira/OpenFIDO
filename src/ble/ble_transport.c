/**
 * @file ble_transport.c
 * @brief BLE Transport Manager Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ble_transport.h"

#include <stdlib.h>
#include <string.h>

#include "../hal/hal_ble.h"
#include "../utils/logger.h"
#include "ble_fido_service.h"
#include "ble_fragment.h"

/* ========== Connection State ========== */

/**
 * @brief Connection state tracking
 */
typedef struct {
    hal_ble_conn_handle_t conn_handle;
    bool is_connected;
    bool is_encrypted;
    bool is_paired;
    uint16_t mtu;
    uint64_t last_activity_ms;
    uint64_t connection_time_ms;
    uint8_t pairing_attempts;
    uint64_t pairing_block_until_ms;
} ble_connection_state_t;

/* ========== Transport State ========== */

typedef struct {
    bool initialized;
    ble_transport_state_t state;
    ble_transport_callbacks_t callbacks;
    ble_connection_state_t connection;
    ble_fragment_buffer_t rx_fragment;
} ble_transport_ctx_t;

static ble_transport_ctx_t transport_state = {
    .initialized = false,
    .state = BLE_TRANSPORT_STATE_IDLE,
    .connection = {.conn_handle = HAL_BLE_CONN_HANDLE_INVALID,
                   .is_connected = false,
                   .is_encrypted = false,
                   .is_paired = false,
                   .mtu = BLE_FRAGMENT_DEFAULT_MTU,
                   .last_activity_ms = 0,
                   .connection_time_ms = 0,
                   .pairing_attempts = 0,
                   .pairing_block_until_ms = 0}};

/* ========== Connection Parameters ========== */

/* Connection interval for active operations (7.5ms - 15ms) */
#define BLE_CONN_INTERVAL_ACTIVE_MIN_MS 8
#define BLE_CONN_INTERVAL_ACTIVE_MAX_MS 15

/* Connection interval for idle state (100ms - 200ms) */
#define BLE_CONN_INTERVAL_IDLE_MIN_MS 100
#define BLE_CONN_INTERVAL_IDLE_MAX_MS 200

/* Connection latency and timeout */
#define BLE_CONN_LATENCY 0
#define BLE_CONN_TIMEOUT_MS 4000

/* Idle timeout before switching to low-power connection params (1 second) */
#define BLE_IDLE_TIMEOUT_MS 1000

/* ========== Security Parameters ========== */

/* Maximum pairing attempts before blocking */
#define BLE_MAX_PAIRING_ATTEMPTS 3

/* Pairing block duration after max attempts (60 seconds) */
#define BLE_PAIRING_BLOCK_DURATION_MS 60000

/* ========== Forward Declarations ========== */

static void on_ble_event(const hal_ble_event_t *event);
static void on_control_point_write(uint16_t conn_handle, const uint8_t *data, size_t len);
static void on_status_notify(uint16_t conn_handle, bool enabled);
static void update_connection_state(void);
static void update_activity_timestamp(void);
static int set_connection_params_active(void);
static int set_connection_params_idle(void);
static uint64_t get_time_ms(void);
static bool is_pairing_blocked(void);
static void handle_pairing_request(const hal_ble_event_t *event);
static void handle_pairing_complete(const hal_ble_event_t *event);
static void handle_pairing_failed(const hal_ble_event_t *event);
static void reset_pairing_attempts(void);

/* ========== Transport Management ========== */

int ble_transport_init(const ble_transport_callbacks_t *callbacks)
{
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

    /* Configure security settings - enforce LE Secure Connections with numeric comparison */
    hal_ble_security_config_t security_config = {
        .mode = HAL_BLE_SECURITY_MODE_AUTHENTICATED,
        .pairing = HAL_BLE_PAIRING_METHOD_NUMERIC,
        .bonding_enabled = true,
        .mitm_protection = true
    };

    ret = hal_ble_set_security_config(&security_config);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to set security config: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    LOG_INFO("BLE security configured: LE Secure Connections with numeric comparison");

    /* Initialize FIDO service */
    ble_fido_service_callbacks_t fido_callbacks = {.on_control_point_write = on_control_point_write,
                                                   .on_status_notify = on_status_notify};

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

int ble_transport_start(void)
{
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

int ble_transport_stop(void)
{
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    LOG_INFO("Stopping BLE transport");

    /* Stop advertising */
    hal_ble_stop_advertising();

    /* Disconnect if connected */
    if (transport_state.connection.conn_handle != HAL_BLE_CONN_HANDLE_INVALID) {
        hal_ble_disconnect(transport_state.connection.conn_handle);
        transport_state.connection.conn_handle = HAL_BLE_CONN_HANDLE_INVALID;
    }

    transport_state.state = BLE_TRANSPORT_STATE_IDLE;

    LOG_INFO("BLE transport stopped");

    return BLE_TRANSPORT_OK;
}

ble_transport_state_t ble_transport_get_state(void)
{
    return transport_state.state;
}

bool ble_transport_is_connected(void)
{
    return transport_state.connection.is_connected &&
           transport_state.connection.conn_handle != HAL_BLE_CONN_HANDLE_INVALID &&
           transport_state.state >= BLE_TRANSPORT_STATE_CONNECTED;
}

/* ========== CTAP Message Processing ========== */

int ble_transport_process_request(const uint8_t *data, size_t len)
{
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

    /* Enforce encryption requirement for CTAP operations */
    if (!transport_state.connection.is_encrypted) {
        LOG_ERROR("Cannot process request: connection not encrypted");
        return BLE_TRANSPORT_ERROR_NOT_ENCRYPTED;
    }

    /* Enforce pairing requirement for CTAP operations */
    if (!transport_state.connection.is_paired) {
        LOG_ERROR("Cannot process request: connection not paired");
        return BLE_TRANSPORT_ERROR_NOT_PAIRED;
    }

    /* Update activity timestamp */
    update_activity_timestamp();

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

        /* Switch to active connection parameters for processing */
        set_connection_params_active();

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

        /* Update activity timestamp after processing */
        update_activity_timestamp();
    }

    return BLE_TRANSPORT_OK;
}

int ble_transport_send_response(const uint8_t *data, size_t len)
{
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

    /* Enforce encryption requirement for CTAP operations */
    if (!transport_state.connection.is_encrypted) {
        LOG_ERROR("Cannot send response: connection not encrypted");
        return BLE_TRANSPORT_ERROR_NOT_ENCRYPTED;
    }

    /* Enforce pairing requirement for CTAP operations */
    if (!transport_state.connection.is_paired) {
        LOG_ERROR("Cannot send response: connection not paired");
        return BLE_TRANSPORT_ERROR_NOT_PAIRED;
    }

    /* Update activity timestamp */
    update_activity_timestamp();

    LOG_INFO("Sending CTAP response: %zu bytes", len);

    /* Fragment the response */
    uint8_t *fragments[BLE_FRAGMENT_MAX_FRAGMENTS];
    size_t fragment_sizes[BLE_FRAGMENT_MAX_FRAGMENTS];
    size_t num_fragments = 0;

    int ret = ble_fragment_create(data, len, fragments, fragment_sizes, &num_fragments,
                                  transport_state.connection.mtu);
    if (ret != BLE_FRAGMENT_OK) {
        LOG_ERROR("Failed to create fragments: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    LOG_DEBUG("Created %zu fragments for response", num_fragments);

    /* Send each fragment */
    for (size_t i = 0; i < num_fragments; i++) {
        ret = ble_fido_service_send_status(transport_state.connection.conn_handle, fragments[i],
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

    /* Update activity timestamp after sending */
    update_activity_timestamp();

    return BLE_TRANSPORT_OK;
}

/* ========== Connection Management ========== */

uint16_t ble_transport_get_connection_handle(void)
{
    return transport_state.connection.conn_handle;
}

uint16_t ble_transport_get_mtu(void)
{
    return transport_state.connection.mtu;
}

bool ble_transport_is_encrypted(void)
{
    return transport_state.connection.is_encrypted;
}

bool ble_transport_is_paired(void)
{
    return transport_state.connection.is_paired;
}

uint64_t ble_transport_get_last_activity_ms(void)
{
    return transport_state.connection.last_activity_ms;
}

int ble_transport_disconnect(void)
{
    if (!transport_state.initialized) {
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (transport_state.connection.conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    int ret = hal_ble_disconnect(transport_state.connection.conn_handle);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to disconnect: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    return BLE_TRANSPORT_OK;
}

/* ========== Helper Functions ========== */

/**
 * @brief Get current time in milliseconds
 *
 * @return Current time in milliseconds
 */
static uint64_t get_time_ms(void)
{
    /* TODO: Implement platform-specific time function */
    /* For now, return a placeholder value */
    /* This should be replaced with hal_get_time_ms() or similar */
    static uint64_t time_counter = 0;
    return time_counter++;
}

/**
 * @brief Update activity timestamp
 */
static void update_activity_timestamp(void)
{
    transport_state.connection.last_activity_ms = get_time_ms();
}

/**
 * @brief Update connection state based on current conditions
 */
static void update_connection_state(void)
{
    if (!transport_state.connection.is_connected) {
        return;
    }

    uint64_t current_time = get_time_ms();
    uint64_t idle_time = current_time - transport_state.connection.last_activity_ms;

    /* Check if we should switch to idle connection parameters */
    if (idle_time >= BLE_IDLE_TIMEOUT_MS &&
        transport_state.state == BLE_TRANSPORT_STATE_CONNECTED) {
        LOG_DEBUG("Connection idle for %llu ms, switching to low-power params", idle_time);
        set_connection_params_idle();
    }
}

/**
 * @brief Set connection parameters for active operations
 *
 * @return 0 on success, negative error code otherwise
 */
static int set_connection_params_active(void)
{
    if (transport_state.connection.conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Setting active connection parameters");

    int ret = hal_ble_update_connection_params(
        transport_state.connection.conn_handle, BLE_CONN_INTERVAL_ACTIVE_MIN_MS,
        BLE_CONN_INTERVAL_ACTIVE_MAX_MS, BLE_CONN_LATENCY, BLE_CONN_TIMEOUT_MS);

    if (ret != HAL_BLE_OK) {
        LOG_WARN("Failed to update connection params: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    return BLE_TRANSPORT_OK;
}

/**
 * @brief Set connection parameters for idle/low-power state
 *
 * @return 0 on success, negative error code otherwise
 */
static int set_connection_params_idle(void)
{
    if (transport_state.connection.conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Setting idle connection parameters");

    int ret = hal_ble_update_connection_params(
        transport_state.connection.conn_handle, BLE_CONN_INTERVAL_IDLE_MIN_MS,
        BLE_CONN_INTERVAL_IDLE_MAX_MS, BLE_CONN_LATENCY, BLE_CONN_TIMEOUT_MS);

    if (ret != HAL_BLE_OK) {
        LOG_WARN("Failed to update connection params: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    return BLE_TRANSPORT_OK;
}

/* ========== Security and Pairing Functions ========== */

/**
 * @brief Check if pairing is currently blocked
 * 
 * @return true if pairing is blocked, false otherwise
 */
static bool is_pairing_blocked(void)
{
    uint64_t current_time = get_time_ms();
    
    if (transport_state.connection.pairing_block_until_ms > current_time) {
        uint64_t remaining_ms = transport_state.connection.pairing_block_until_ms - current_time;
        LOG_WARN("Pairing blocked for %llu more seconds", remaining_ms / 1000);
        return true;
    }
    
    return false;
}

/**
 * @brief Reset pairing attempts counter
 */
static void reset_pairing_attempts(void)
{
    transport_state.connection.pairing_attempts = 0;
    transport_state.connection.pairing_block_until_ms = 0;
}

/**
 * @brief Handle pairing request event
 * 
 * @param event BLE event
 */
static void handle_pairing_request(const hal_ble_event_t *event)
{
    LOG_INFO("Pairing request received");
    
    /* Check if pairing is blocked */
    if (is_pairing_blocked()) {
        LOG_ERROR("Pairing blocked due to too many failed attempts");
        
        /* Reject the pairing request */
        hal_ble_pairing_confirm(event->conn_handle, false);
        return;
    }
    
    /* For numeric comparison, the HAL will generate a passkey and display it */
    /* The user must confirm the passkey matches on both devices */
    LOG_INFO("Pairing method: Numeric Comparison");
    LOG_INFO("Pairing attempts: %d/%d", 
             transport_state.connection.pairing_attempts + 1, 
             BLE_MAX_PAIRING_ATTEMPTS);
}

/**
 * @brief Handle pairing complete event
 * 
 * @param event BLE event
 */
static void handle_pairing_complete(const hal_ble_event_t *event)
{
    LOG_INFO("Pairing completed successfully");
    
    /* Reset pairing attempts on success */
    reset_pairing_attempts();
    
    /* Update connection state */
    transport_state.connection.is_paired = true;
    
    /* Verify encryption is enabled after pairing */
    bool encrypted = false;
    if (hal_ble_is_encrypted(event->conn_handle, &encrypted) == HAL_BLE_OK) {
        transport_state.connection.is_encrypted = encrypted;
        if (!encrypted) {
            LOG_ERROR("Pairing complete but connection not encrypted!");
            /* Disconnect if encryption is not enabled */
            hal_ble_disconnect(event->conn_handle);
        } else {
            LOG_INFO("Connection is now encrypted and paired");
        }
    }
}

/**
 * @brief Handle pairing failed event
 * 
 * @param event BLE event
 */
static void handle_pairing_failed(const hal_ble_event_t *event)
{
    LOG_ERROR("Pairing failed: error=%d", event->error_code);
    
    /* Increment pairing attempts */
    transport_state.connection.pairing_attempts++;
    
    LOG_WARN("Pairing attempt %d/%d failed", 
             transport_state.connection.pairing_attempts, 
             BLE_MAX_PAIRING_ATTEMPTS);
    
    /* Check if we've reached the maximum attempts */
    if (transport_state.connection.pairing_attempts >= BLE_MAX_PAIRING_ATTEMPTS) {
        uint64_t current_time = get_time_ms();
        transport_state.connection.pairing_block_until_ms = 
            current_time + BLE_PAIRING_BLOCK_DURATION_MS;
        
        LOG_ERROR("Maximum pairing attempts reached. Blocking pairing for %d seconds",
                  BLE_PAIRING_BLOCK_DURATION_MS / 1000);
        
        /* Disconnect the connection */
        hal_ble_disconnect(event->conn_handle);
    }
    
    transport_state.connection.is_paired = false;
}

/* ========== Event Handlers ========== */

static void on_ble_event(const hal_ble_event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->type) {
        case HAL_BLE_EVENT_CONNECTED:
            LOG_INFO("BLE connected: conn_handle=%d", event->conn_handle);

            /* Update connection state */
            transport_state.connection.conn_handle = event->conn_handle;
            transport_state.connection.is_connected = true;
            transport_state.connection.is_encrypted = false;
            transport_state.connection.is_paired = false;
            transport_state.connection.connection_time_ms = get_time_ms();
            update_activity_timestamp();

            /* Note: pairing_attempts and pairing_block_until_ms are NOT reset here */
            /* They persist across connections to enforce the blocking period */

            /* Get MTU */
            hal_ble_get_mtu(event->conn_handle, &transport_state.connection.mtu);
            LOG_INFO("Connection MTU: %d", transport_state.connection.mtu);

            /* Set active connection parameters for initial connection */
            set_connection_params_active();

            /* Update transport state */
            transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;

            /* Reset fragment buffer */
            ble_fragment_reset(&transport_state.rx_fragment);

            /* Notify callback */
            if (transport_state.callbacks.on_connection_change != NULL) {
                transport_state.callbacks.on_connection_change(true);
            }
            break;

        case HAL_BLE_EVENT_DISCONNECTED:
            LOG_INFO("BLE disconnected: conn_handle=%d", event->conn_handle);

            /* Reset connection state */
            transport_state.connection.conn_handle = HAL_BLE_CONN_HANDLE_INVALID;
            transport_state.connection.is_connected = false;
            transport_state.connection.is_encrypted = false;
            transport_state.connection.is_paired = false;
            transport_state.connection.last_activity_ms = 0;
            transport_state.connection.connection_time_ms = 0;

            /* Note: pairing_attempts and pairing_block_until_ms are NOT reset here */
            /* They persist across connections to enforce the blocking period */

            /* Update transport state */
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
            /* Update activity timestamp */
            update_activity_timestamp();

            /* Handle Control Point writes */
            if (event->char_handle == ble_fido_service_get_control_point_handle()) {
                ble_fido_service_on_control_point_write(event->conn_handle, event->data,
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
            transport_state.connection.mtu = event->mtu;
            break;

        case HAL_BLE_EVENT_ENCRYPTION_CHANGED:
            LOG_INFO("Encryption changed: encrypted=%d", event->encrypted);
            transport_state.connection.is_encrypted = event->encrypted;

            /* Check encryption status */
            if (!event->encrypted) {
                LOG_WARN("Connection is not encrypted");
            }
            break;

        case HAL_BLE_EVENT_PAIRING_REQUEST:
            handle_pairing_request(event);
            break;

        case HAL_BLE_EVENT_PAIRING_COMPLETE:
            handle_pairing_complete(event);
            break;

        case HAL_BLE_EVENT_PAIRING_FAILED:
            handle_pairing_failed(event);
            break;

        default:
            break;
    }
}

static void on_control_point_write(uint16_t conn_handle, const uint8_t *data, size_t len)
{
    LOG_DEBUG("Control Point write: conn=%d, len=%zu", conn_handle, len);

    /* Process the fragment */
    ble_transport_process_request(data, len);
}

static void on_status_notify(uint16_t conn_handle, bool enabled)
{
    LOG_INFO("Status notify %s: conn=%d", enabled ? "enabled" : "disabled", conn_handle);
}
