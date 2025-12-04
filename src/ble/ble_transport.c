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

#include "../hal/hal.h"
#include "../hal/hal_ble.h"
#include "../transport/transport.h"
#include "../utils/led_patterns.h"
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
    bool low_power_mode;
    uint64_t last_global_activity_ms;
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
                   .pairing_block_until_ms = 0},
    .low_power_mode = false,
    .last_global_activity_ms = 0};

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

/* ========== Power Management Parameters ========== */

/* Low-power advertising intervals (1000ms - 2000ms) */
#define BLE_ADV_INTERVAL_LOW_POWER_MIN_MS 1000
#define BLE_ADV_INTERVAL_LOW_POWER_MAX_MS 2000

/* Normal advertising intervals (100ms - 200ms) */
#define BLE_ADV_INTERVAL_NORMAL_MIN_MS 100
#define BLE_ADV_INTERVAL_NORMAL_MAX_MS 200

/* Deep sleep timeout (5 minutes of inactivity) */
#define BLE_DEEP_SLEEP_TIMEOUT_MS (5 * 60 * 1000)

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
static int enter_low_power_mode(void);
static int exit_low_power_mode(void);
static void update_global_activity_timestamp(void);
static void cleanup_operation_state(void);
static int send_ctap_error_response(uint8_t error_code);
static const char *ble_transport_error_to_string(int error_code);
static const char *hal_ble_error_to_string(int error_code);

/* ========== Transport Management ========== */

int ble_transport_init(const ble_transport_callbacks_t *callbacks)
{
    if (callbacks == NULL) {
        LOG_ERROR("BLE transport init failed: NULL callbacks");
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (transport_state.initialized) {
        LOG_WARN("BLE transport already initialized");
        return BLE_TRANSPORT_OK;
    }

    LOG_INFO("Initializing BLE transport");

    /* Check if BLE is supported */
    if (!hal_ble_is_supported()) {
        LOG_ERROR("BLE not supported on this platform");
        return BLE_TRANSPORT_ERROR;
    }

    /* Store callbacks */
    transport_state.callbacks = *callbacks;

    /* Initialize BLE HAL */
    int ret = hal_ble_init(on_ble_event);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("BLE HAL init failed: %s (code=%d) - BLE transport unavailable",
                  hal_ble_error_to_string(ret), ret);
        /* Clean up state on failure */
        memset(&transport_state.callbacks, 0, sizeof(transport_state.callbacks));
        return BLE_TRANSPORT_ERROR;
    }

    /* Configure security settings - enforce LE Secure Connections with numeric comparison */
    hal_ble_security_config_t security_config = {.mode = HAL_BLE_SECURITY_MODE_AUTHENTICATED,
                                                 .pairing = HAL_BLE_PAIRING_METHOD_NUMERIC,
                                                 .bonding_enabled = true,
                                                 .mitm_protection = true};

    ret = hal_ble_set_security_config(&security_config);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to set security config: %s (code=%d) - continuing with default security",
                  hal_ble_error_to_string(ret), ret);
        /* Continue initialization - security config failure is not fatal */
        /* The HAL should have reasonable defaults */
    } else {
        LOG_INFO("BLE security configured: LE Secure Connections with numeric comparison");
    }

    /* Initialize FIDO service */
    ble_fido_service_callbacks_t fido_callbacks = {.on_control_point_write = on_control_point_write,
                                                   .on_status_notify = on_status_notify};

    ret = ble_fido_service_init(&fido_callbacks);
    if (ret != BLE_FIDO_SERVICE_OK) {
        LOG_ERROR("FIDO service init failed: code=%d - BLE transport unavailable", ret);
        /* Clean up HAL on FIDO service failure */
        LOG_INFO("Cleaning up BLE HAL after FIDO service init failure");
        hal_ble_deinit();
        memset(&transport_state.callbacks, 0, sizeof(transport_state.callbacks));
        return BLE_TRANSPORT_ERROR;
    }

    /* Initialize fragment buffer */
    ble_fragment_init(&transport_state.rx_fragment);

    transport_state.initialized = true;
    transport_state.state = BLE_TRANSPORT_STATE_IDLE;

    LOG_INFO("BLE transport initialized successfully");

    return BLE_TRANSPORT_OK;
}

int ble_transport_start(void)
{
    if (!transport_state.initialized) {
        LOG_ERROR("BLE transport not initialized - cannot start");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (transport_state.state != BLE_TRANSPORT_STATE_IDLE) {
        LOG_WARN("BLE transport already started (state=%d)", transport_state.state);
        return BLE_TRANSPORT_OK;
    }

    LOG_INFO("Starting BLE transport");

    /* Start advertising with low-power intervals */
    hal_ble_adv_params_t adv_params = {.interval_min_ms = BLE_ADV_INTERVAL_LOW_POWER_MIN_MS,
                                       .interval_max_ms = BLE_ADV_INTERVAL_LOW_POWER_MAX_MS,
                                       .timeout_s = 0,
                                       .connectable = true};

    int ret = hal_ble_start_advertising(&adv_params);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to start advertising: %s (code=%d) - attempting recovery",
                  hal_ble_error_to_string(ret), ret);
        /* Try to recover by reinitializing the BLE stack */
        LOG_INFO("Attempting to recover BLE stack...");
        hal_ble_deinit();
        ret = hal_ble_init(on_ble_event);
        if (ret == HAL_BLE_OK) {
            LOG_INFO("BLE HAL reinitialized, retrying advertising");
            ret = hal_ble_start_advertising(&adv_params);
            if (ret != HAL_BLE_OK) {
                LOG_ERROR("Recovery failed: %s (code=%d) - BLE transport unavailable",
                          hal_ble_error_to_string(ret), ret);
                return BLE_TRANSPORT_ERROR;
            }
            LOG_INFO("BLE stack recovered successfully");
        } else {
            LOG_ERROR("BLE stack recovery failed: %s (code=%d)", hal_ble_error_to_string(ret), ret);
            return BLE_TRANSPORT_ERROR;
        }
    }

    transport_state.state = BLE_TRANSPORT_STATE_ADVERTISING;
    transport_state.low_power_mode = true;

    /* Update global activity timestamp */
    update_global_activity_timestamp();

    /* Set LED pattern for advertising (slow blink) */
    led_set_pattern(LED_PATTERN_BLE_ADVERTISING);

    LOG_INFO("BLE transport started (advertising in low-power mode)");

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

    /* Turn off BLE LED indication when stopped */
    led_set_pattern(LED_PATTERN_IDLE);

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
        LOG_ERROR("BLE transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        LOG_ERROR("Invalid parameters: data=%p, len=%zu", (void *) data, len);
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    /* Validate data length to prevent buffer overflow */
    if (len > BLE_FRAGMENT_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Data too large: %zu bytes (max %d) - rejecting request", len,
                  BLE_FRAGMENT_MAX_MESSAGE_SIZE);
        cleanup_operation_state();
        /* Send CTAP error response for invalid length */
        send_ctap_error_response(0x03); /* CTAP2_ERR_INVALID_LENGTH */
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!ble_transport_is_connected()) {
        LOG_ERROR("Cannot process request: not connected (conn_handle=%d, state=%d)",
                  transport_state.connection.conn_handle, transport_state.state);
        cleanup_operation_state();
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    /* Enforce encryption requirement for CTAP operations */
    if (!transport_state.connection.is_encrypted) {
        LOG_ERROR("Cannot process request: connection not encrypted (conn_handle=%d, paired=%d)",
                  transport_state.connection.conn_handle, transport_state.connection.is_paired);
        cleanup_operation_state();
        /* Send CTAP error response for unauthorized access */
        send_ctap_error_response(0x40); /* CTAP2_ERR_UNAUTHORIZED_PERMISSION */
        return BLE_TRANSPORT_ERROR_NOT_ENCRYPTED;
    }

    /* Enforce pairing requirement for CTAP operations */
    if (!transport_state.connection.is_paired) {
        LOG_ERROR("Cannot process request: connection not paired (conn_handle=%d, encrypted=%d)",
                  transport_state.connection.conn_handle, transport_state.connection.is_encrypted);
        cleanup_operation_state();
        /* Send CTAP error response for unauthorized access */
        send_ctap_error_response(0x40); /* CTAP2_ERR_UNAUTHORIZED_PERMISSION */
        return BLE_TRANSPORT_ERROR_NOT_PAIRED;
    }

    /* Update activity timestamp */
    update_activity_timestamp();

    LOG_DEBUG("Processing CTAP request fragment: len=%zu", len);

    /* Add fragment to reassembly buffer */
    int ret = ble_fragment_add(&transport_state.rx_fragment, data, len);
    if (ret != BLE_FRAGMENT_OK) {
        LOG_ERROR("Fragment add failed: error=%d, conn_handle=%d, fragment_len=%zu", ret,
                  transport_state.connection.conn_handle, len);

        /* Handle specific fragment errors and send appropriate CTAP error */
        uint8_t ctap_error = 0x01; /* CTAP2_ERR_INVALID_COMMAND (default) */
        switch (ret) {
            case BLE_FRAGMENT_ERROR_INVALID_PARAM:
                LOG_ERROR("Invalid fragment data received (malformed fragment)");
                ctap_error = 0x02; /* CTAP2_ERR_INVALID_PARAMETER */
                break;
            case BLE_FRAGMENT_ERROR_INVALID_SEQ:
                LOG_ERROR("Fragment sequence error (expected seq=%d) - resetting buffer",
                          transport_state.rx_fragment.seq);
                ctap_error = 0x04; /* CTAP2_ERR_INVALID_SEQ */
                break;
            case BLE_FRAGMENT_ERROR_TOO_LARGE:
                LOG_ERROR("Fragment message too large (total_len=%zu, max=%d) - resetting buffer",
                          transport_state.rx_fragment.total_len, BLE_FRAGMENT_MAX_MESSAGE_SIZE);
                ctap_error = 0x39; /* CTAP2_ERR_REQUEST_TOO_LARGE */
                break;
            default:
                LOG_ERROR("Unknown fragment error: %d", ret);
                break;
        }

        /* Clean up and send error response */
        cleanup_operation_state();
        send_ctap_error_response(ctap_error);
        return BLE_TRANSPORT_ERROR;
    }

    /* Check if message is complete */
    if (ble_fragment_is_complete(&transport_state.rx_fragment)) {
        uint8_t *msg_data;
        size_t msg_len;

        ret = ble_fragment_get_message(&transport_state.rx_fragment, &msg_data, &msg_len);
        if (ret != BLE_FRAGMENT_OK) {
            LOG_ERROR("Failed to get complete message: error=%d, received=%zu/%zu", ret,
                      transport_state.rx_fragment.received_len,
                      transport_state.rx_fragment.total_len);
            cleanup_operation_state();
            send_ctap_error_response(0x01); /* CTAP2_ERR_INVALID_COMMAND */
            return BLE_TRANSPORT_ERROR;
        }

        LOG_INFO("CTAP request complete: %zu bytes (conn_handle=%d)", msg_len,
                 transport_state.connection.conn_handle);

        /* Check if another transport has an operation in progress */
        if (transport_is_busy() && !transport_is_active(TRANSPORT_TYPE_BLE)) {
            LOG_WARN(
                "Operation in progress on another transport, rejecting BLE request "
                "(active_transport=%d)",
                transport_get_active());
            cleanup_operation_state();
            /* Send CTAP error response for channel busy */
            send_ctap_error_response(0x06); /* CTAP2_ERR_CHANNEL_BUSY */
            return BLE_TRANSPORT_ERROR_BUSY;
        }

        /* Set BLE as active transport and lock it for this operation */
        transport_set_active(TRANSPORT_TYPE_BLE);
        transport_lock(TRANSPORT_TYPE_BLE);
        transport_set_busy(true);

        /* Switch to active connection parameters for processing */
        set_connection_params_active();

        /* Update state */
        transport_state.state = BLE_TRANSPORT_STATE_PROCESSING;

        /* Set LED pattern for processing (fast blink) */
        led_set_pattern(LED_PATTERN_BLE_PROCESSING);

        /* Invoke callback */
        if (transport_state.callbacks.on_ctap_request != NULL) {
            transport_state.callbacks.on_ctap_request(msg_data, msg_len);
        }

        /* Reset fragment buffer for next message */
        ble_fragment_reset(&transport_state.rx_fragment);

        /* Return to connected state */
        transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;

        /* Restore LED pattern to connected (solid on) */
        led_set_pattern(LED_PATTERN_BLE_CONNECTED);

        /* Clear operation state */
        transport_set_busy(false);
        transport_unlock();

        /* Update activity timestamp after processing */
        update_activity_timestamp();

        /* Return to idle connection parameters after operation completes */
        set_connection_params_idle();
    }

    return BLE_TRANSPORT_OK;
}

int ble_transport_send_response(const uint8_t *data, size_t len)
{
    if (!transport_state.initialized) {
        LOG_ERROR("Cannot send response: transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        LOG_ERROR("Cannot send response: invalid parameters (data=%p, len=%zu)", (void *) data,
                  len);
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    /* Validate response size */
    if (len > BLE_FRAGMENT_MAX_MESSAGE_SIZE) {
        LOG_ERROR("Cannot send response: data too large (%zu bytes, max %d)", len,
                  BLE_FRAGMENT_MAX_MESSAGE_SIZE);
        return BLE_TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!ble_transport_is_connected()) {
        LOG_ERROR("Cannot send response: not connected (conn_handle=%d)",
                  transport_state.connection.conn_handle);
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    /* Enforce encryption requirement for CTAP operations */
    if (!transport_state.connection.is_encrypted) {
        LOG_ERROR("Cannot send response: connection not encrypted (conn_handle=%d)",
                  transport_state.connection.conn_handle);
        return BLE_TRANSPORT_ERROR_NOT_ENCRYPTED;
    }

    /* Enforce pairing requirement for CTAP operations */
    if (!transport_state.connection.is_paired) {
        LOG_ERROR("Cannot send response: connection not paired (conn_handle=%d)",
                  transport_state.connection.conn_handle);
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
        LOG_ERROR("Failed to create fragments: error=%d, len=%zu, mtu=%d, conn_handle=%d", ret, len,
                  transport_state.connection.mtu, transport_state.connection.conn_handle);

        /* Log specific fragment creation errors */
        if (ret == BLE_FRAGMENT_ERROR_NO_MEM) {
            LOG_ERROR("Out of memory while creating fragments");
        } else if (ret == BLE_FRAGMENT_ERROR_TOO_LARGE) {
            LOG_ERROR("Response too large to fragment (exceeds max fragments)");
        }

        return BLE_TRANSPORT_ERROR;
    }

    LOG_DEBUG("Created %zu fragments for response", num_fragments);

    /* Send each fragment */
    for (size_t i = 0; i < num_fragments; i++) {
        /* Verify connection is still valid before sending each fragment */
        if (!ble_transport_is_connected()) {
            LOG_ERROR("Connection lost while sending fragment %zu/%zu (conn_handle=%d, state=%d)",
                      i + 1, num_fragments, transport_state.connection.conn_handle,
                      transport_state.state);

            /* Clean up remaining fragments */
            for (size_t j = i; j < num_fragments; j++) {
                free(fragments[j]);
            }

            return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
        }

        ret = ble_fido_service_send_status(transport_state.connection.conn_handle, fragments[i],
                                           fragment_sizes[i]);
        if (ret != BLE_FIDO_SERVICE_OK) {
            LOG_ERROR("Failed to send fragment %zu/%zu: error=%d, size=%zu, conn_handle=%d", i + 1,
                      num_fragments, ret, fragment_sizes[i],
                      transport_state.connection.conn_handle);

            /* Clean up remaining fragments */
            for (size_t j = i; j < num_fragments; j++) {
                free(fragments[j]);
            }

            return BLE_TRANSPORT_ERROR;
        }

        LOG_DEBUG("Sent fragment %zu/%zu: %zu bytes", i + 1, num_fragments, fragment_sizes[i]);

        /* Free fragment after sending */
        free(fragments[i]);
    }

    LOG_INFO("CTAP response sent successfully (%zu fragments)", num_fragments);

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
        LOG_ERROR("Cannot disconnect: transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (transport_state.connection.conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        LOG_WARN("Cannot disconnect: no active connection");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_INFO("Disconnecting BLE connection: conn_handle=%d",
             transport_state.connection.conn_handle);

    int ret = hal_ble_disconnect(transport_state.connection.conn_handle);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to disconnect: %s (code=%d, conn_handle=%d)",
                  hal_ble_error_to_string(ret), ret, transport_state.connection.conn_handle);
        return BLE_TRANSPORT_ERROR;
    }

    LOG_INFO("Disconnect initiated successfully");

    return BLE_TRANSPORT_OK;
}

/* ========== Power Management ========== */

bool ble_transport_should_enter_deep_sleep(void)
{
    if (!transport_state.initialized) {
        return false;
    }

    /* Don't sleep if connected or processing */
    if (transport_state.connection.is_connected ||
        transport_state.state == BLE_TRANSPORT_STATE_PROCESSING) {
        return false;
    }

    uint64_t current_time = get_time_ms();
    uint64_t idle_time = current_time - transport_state.last_global_activity_ms;

    /* Check if we've been idle long enough for deep sleep */
    return (idle_time >= BLE_DEEP_SLEEP_TIMEOUT_MS);
}

void ble_transport_update_power_state(void)
{
    if (!transport_state.initialized) {
        return;
    }

    update_connection_state();
}

int ble_transport_enter_deep_sleep(void)
{
    if (!transport_state.initialized) {
        LOG_ERROR("Cannot enter deep sleep: transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    /* Don't enter deep sleep if connected or processing */
    if (transport_state.connection.is_connected ||
        transport_state.state == BLE_TRANSPORT_STATE_PROCESSING) {
        LOG_WARN("Cannot enter deep sleep: device is active (connected=%d, state=%d)",
                 transport_state.connection.is_connected, transport_state.state);
        return BLE_TRANSPORT_ERROR_BUSY;
    }

    LOG_INFO("Entering deep sleep mode (idle_time=%llu ms)",
             get_time_ms() - transport_state.last_global_activity_ms);

    /* Stop advertising before deep sleep */
    hal_ble_stop_advertising();

    /* Enter deep sleep via HAL */
    int ret = hal_ble_enter_deep_sleep();
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to enter deep sleep: %s (code=%d) - restarting advertising",
                  hal_ble_error_to_string(ret), ret);
        /* Restart advertising if deep sleep failed */
        ret = hal_ble_start_advertising(NULL);
        if (ret != HAL_BLE_OK) {
            LOG_ERROR("Failed to restart advertising after deep sleep failure: %s (code=%d)",
                      hal_ble_error_to_string(ret), ret);
        }
        return BLE_TRANSPORT_ERROR;
    }

    /* Update state */
    transport_state.state = BLE_TRANSPORT_STATE_IDLE;

    /* Turn off LED */
    led_set_pattern(LED_PATTERN_IDLE);

    LOG_INFO("Deep sleep mode entered successfully");

    return BLE_TRANSPORT_OK;
}

int ble_transport_wake_from_deep_sleep(void)
{
    if (!transport_state.initialized) {
        LOG_ERROR("Cannot wake from deep sleep: transport not initialized");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    LOG_INFO("Waking from deep sleep mode");

    /* Wake from deep sleep via HAL */
    int ret = hal_ble_wake_from_sleep();
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to wake from deep sleep: %s (code=%d)", hal_ble_error_to_string(ret),
                  ret);
        return BLE_TRANSPORT_ERROR;
    }

    /* Update activity timestamp */
    update_global_activity_timestamp();

    /* Restart advertising */
    ret = ble_transport_start();
    if (ret != BLE_TRANSPORT_OK) {
        LOG_ERROR("Failed to restart advertising after wake: %s",
                  ble_transport_error_to_string(ret));
        return BLE_TRANSPORT_ERROR;
    }

    LOG_INFO("Woke from deep sleep mode successfully");

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
    return hal_get_timestamp_ms();
}

/**
 * @brief Update activity timestamp
 */
static void update_activity_timestamp(void)
{
    transport_state.connection.last_activity_ms = get_time_ms();
    update_global_activity_timestamp();
}

/**
 * @brief Update connection state based on current conditions
 */
static void update_connection_state(void)
{
    uint64_t current_time = get_time_ms();

    /* Handle connected state power management */
    if (transport_state.connection.is_connected) {
        uint64_t idle_time = current_time - transport_state.connection.last_activity_ms;

        /* Check if we should switch to idle connection parameters */
        if (idle_time >= BLE_IDLE_TIMEOUT_MS &&
            transport_state.state == BLE_TRANSPORT_STATE_CONNECTED) {
            LOG_DEBUG("Connection idle for %llu ms, switching to low-power params", idle_time);
            set_connection_params_idle();
        }
    }
    /* Handle advertising state power management */
    else if (transport_state.state == BLE_TRANSPORT_STATE_ADVERTISING) {
        uint64_t idle_time = current_time - transport_state.last_global_activity_ms;

        /* Enter low-power advertising after idle timeout */
        if (idle_time >= BLE_IDLE_TIMEOUT_MS && !transport_state.low_power_mode) {
            LOG_DEBUG("Advertising idle for %llu ms, entering low-power mode", idle_time);
            enter_low_power_mode();
        }
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
        LOG_WARN("Cannot set active params: no connection");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Setting active connection parameters (interval=%d-%dms, conn_handle=%d)",
              BLE_CONN_INTERVAL_ACTIVE_MIN_MS, BLE_CONN_INTERVAL_ACTIVE_MAX_MS,
              transport_state.connection.conn_handle);

    int ret = hal_ble_update_connection_params(
        transport_state.connection.conn_handle, BLE_CONN_INTERVAL_ACTIVE_MIN_MS,
        BLE_CONN_INTERVAL_ACTIVE_MAX_MS, BLE_CONN_LATENCY, BLE_CONN_TIMEOUT_MS);

    if (ret != HAL_BLE_OK) {
        LOG_WARN("Failed to update connection params to active: %s (code=%d, conn_handle=%d)",
                 hal_ble_error_to_string(ret), ret, transport_state.connection.conn_handle);
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
        LOG_WARN("Cannot set idle params: no connection");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Setting idle connection parameters (interval=%d-%dms, conn_handle=%d)",
              BLE_CONN_INTERVAL_IDLE_MIN_MS, BLE_CONN_INTERVAL_IDLE_MAX_MS,
              transport_state.connection.conn_handle);

    int ret = hal_ble_update_connection_params(
        transport_state.connection.conn_handle, BLE_CONN_INTERVAL_IDLE_MIN_MS,
        BLE_CONN_INTERVAL_IDLE_MAX_MS, BLE_CONN_LATENCY, BLE_CONN_TIMEOUT_MS);

    if (ret != HAL_BLE_OK) {
        LOG_WARN("Failed to update connection params to idle: %s (code=%d, conn_handle=%d)",
                 hal_ble_error_to_string(ret), ret, transport_state.connection.conn_handle);
        return BLE_TRANSPORT_ERROR;
    }

    return BLE_TRANSPORT_OK;
}

/* ========== Power Management Functions ========== */

/**
 * @brief Update global activity timestamp
 *
 * Updates the global activity timestamp used for deep sleep timeout.
 */
static void update_global_activity_timestamp(void)
{
    transport_state.last_global_activity_ms = get_time_ms();
}

/**
 * @brief Enter low-power mode
 *
 * Switches to low-power advertising intervals when not connected.
 *
 * @return 0 on success, negative error code otherwise
 */
static int enter_low_power_mode(void)
{
    if (transport_state.low_power_mode) {
        return BLE_TRANSPORT_OK; /* Already in low-power mode */
    }

    LOG_INFO("Entering low-power mode (state=%d)", transport_state.state);

    /* Configure low-power advertising if not connected */
    if (transport_state.state == BLE_TRANSPORT_STATE_ADVERTISING) {
        int ret = hal_ble_set_low_power_advertising(true);
        if (ret != HAL_BLE_OK) {
            LOG_WARN("Failed to set low-power advertising: %s (code=%d)",
                     hal_ble_error_to_string(ret), ret);
            return BLE_TRANSPORT_ERROR;
        }
    }

    transport_state.low_power_mode = true;

    LOG_INFO("Low-power mode enabled successfully");

    return BLE_TRANSPORT_OK;
}

/**
 * @brief Exit low-power mode
 *
 * Switches to normal advertising intervals for faster connection.
 *
 * @return 0 on success, negative error code otherwise
 */
static int exit_low_power_mode(void)
{
    if (!transport_state.low_power_mode) {
        return BLE_TRANSPORT_OK; /* Already in normal mode */
    }

    LOG_INFO("Exiting low-power mode (state=%d)", transport_state.state);

    /* Configure normal advertising if not connected */
    if (transport_state.state == BLE_TRANSPORT_STATE_ADVERTISING) {
        int ret = hal_ble_set_low_power_advertising(false);
        if (ret != HAL_BLE_OK) {
            LOG_WARN("Failed to set normal advertising: %s (code=%d)", hal_ble_error_to_string(ret),
                     ret);
            return BLE_TRANSPORT_ERROR;
        }
    }

    transport_state.low_power_mode = false;

    LOG_INFO("Low-power mode disabled successfully");

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
    LOG_INFO("Pairing attempts: %d/%d", transport_state.connection.pairing_attempts + 1,
             BLE_MAX_PAIRING_ATTEMPTS);
}

/**
 * @brief Handle pairing complete event
 *
 * @param event BLE event
 */
static void handle_pairing_complete(const hal_ble_event_t *event)
{
    LOG_INFO("Pairing completed successfully (conn_handle=%d)", event->conn_handle);

    /* Reset pairing attempts on success */
    reset_pairing_attempts();

    /* Update connection state */
    transport_state.connection.is_paired = true;

    /* Verify encryption is enabled after pairing */
    bool encrypted = false;
    int ret = hal_ble_is_encrypted(event->conn_handle, &encrypted);
    if (ret == HAL_BLE_OK) {
        transport_state.connection.is_encrypted = encrypted;
        if (!encrypted) {
            LOG_ERROR(
                "Pairing complete but connection not encrypted! (conn_handle=%d) - "
                "SECURITY VIOLATION",
                event->conn_handle);
            /* Disconnect if encryption is not enabled */
            ret = hal_ble_disconnect(event->conn_handle);
            if (ret != HAL_BLE_OK) {
                LOG_ERROR("Failed to disconnect unencrypted connection: %s (code=%d)",
                          hal_ble_error_to_string(ret), ret);
            }
        } else {
            LOG_INFO("Connection is now encrypted and paired (conn_handle=%d)", event->conn_handle);
        }
    } else {
        LOG_ERROR("Failed to check encryption status after pairing: %s (code=%d, conn_handle=%d)",
                  hal_ble_error_to_string(ret), ret, event->conn_handle);
        /* Disconnect on error to be safe */
        hal_ble_disconnect(event->conn_handle);
    }
}

/**
 * @brief Handle pairing failed event
 *
 * @param event BLE event
 */
static void handle_pairing_failed(const hal_ble_event_t *event)
{
    LOG_ERROR("Pairing failed: error=0x%02X (conn_handle=%d)", event->error_code,
              event->conn_handle);

    /* Increment pairing attempts */
    transport_state.connection.pairing_attempts++;

    LOG_WARN("Pairing attempt %d/%d failed (conn_handle=%d)",
             transport_state.connection.pairing_attempts, BLE_MAX_PAIRING_ATTEMPTS,
             event->conn_handle);

    /* Check if we've reached the maximum attempts */
    if (transport_state.connection.pairing_attempts >= BLE_MAX_PAIRING_ATTEMPTS) {
        uint64_t current_time = get_time_ms();
        transport_state.connection.pairing_block_until_ms =
            current_time + BLE_PAIRING_BLOCK_DURATION_MS;

        LOG_ERROR(
            "Maximum pairing attempts reached (%d/%d). Blocking pairing for %d seconds "
            "(until timestamp=%llu)",
            transport_state.connection.pairing_attempts, BLE_MAX_PAIRING_ATTEMPTS,
            BLE_PAIRING_BLOCK_DURATION_MS / 1000,
            transport_state.connection.pairing_block_until_ms);

        /* Disconnect the connection */
        int ret = hal_ble_disconnect(event->conn_handle);
        if (ret != HAL_BLE_OK) {
            LOG_ERROR("Failed to disconnect after max pairing attempts: %s (code=%d)",
                      hal_ble_error_to_string(ret), ret);
        }
    }

    transport_state.connection.is_paired = false;
}

/* ========== Event Handlers ========== */

static void on_ble_event(const hal_ble_event_t *event)
{
    if (event == NULL) {
        LOG_ERROR("BLE event handler called with NULL event");
        return;
    }

    switch (event->type) {
        case HAL_BLE_EVENT_CONNECTED:
            LOG_INFO("BLE connected: conn_handle=%d", event->conn_handle);

            /* Exit low-power mode on connection */
            if (exit_low_power_mode() != BLE_TRANSPORT_OK) {
                LOG_WARN("Failed to exit low-power mode on connection");
            }

            /* Update connection state */
            transport_state.connection.conn_handle = event->conn_handle;
            transport_state.connection.is_connected = true;
            transport_state.connection.is_encrypted = false;
            transport_state.connection.is_paired = false;
            transport_state.connection.connection_time_ms = get_time_ms();
            update_activity_timestamp();

            /* Note: pairing_attempts and pairing_block_until_ms are NOT reset here */
            /* They persist across connections to enforce the blocking period */

            /* Get MTU with error handling */
            int ret = hal_ble_get_mtu(event->conn_handle, &transport_state.connection.mtu);
            if (ret != HAL_BLE_OK) {
                LOG_ERROR("Failed to get MTU: %d, using default", ret);
                transport_state.connection.mtu = BLE_FRAGMENT_DEFAULT_MTU;
            } else {
                LOG_INFO("Connection MTU: %d", transport_state.connection.mtu);
            }

            /* Set active connection parameters for initial connection */
            if (set_connection_params_active() != BLE_TRANSPORT_OK) {
                LOG_WARN("Failed to set active connection parameters");
            }

            /* Update transport state */
            transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;

            /* Set LED pattern for connected state (solid on) */
            led_set_pattern(LED_PATTERN_BLE_CONNECTED);

            /* Reset fragment buffer to ensure clean state */
            ble_fragment_reset(&transport_state.rx_fragment);

            /* Notify callback */
            if (transport_state.callbacks.on_connection_change != NULL) {
                transport_state.callbacks.on_connection_change(true);
            }
            break;

        case HAL_BLE_EVENT_DISCONNECTED:
            LOG_INFO("BLE disconnected: conn_handle=%d, reason=0x%02X", event->conn_handle,
                     event->error_code);

            /* Check if we were processing a request when disconnected */
            if (transport_state.state == BLE_TRANSPORT_STATE_PROCESSING) {
                LOG_ERROR("Connection lost during CTAP operation (reason=0x%02X) - cleaning up",
                          event->error_code);
                /* Clean up operation state */
                cleanup_operation_state();
            }

            /* Check if fragment reassembly was in progress */
            if (transport_state.rx_fragment.in_progress) {
                LOG_WARN(
                    "Connection lost during fragment reassembly (received=%zu/%zu) - "
                    "discarding partial message",
                    transport_state.rx_fragment.received_len,
                    transport_state.rx_fragment.total_len);
                ble_fragment_reset(&transport_state.rx_fragment);
            }

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

            /* Set LED pattern back to advertising (slow blink) */
            led_set_pattern(LED_PATTERN_BLE_ADVERTISING);

            /* Notify callback */
            if (transport_state.callbacks.on_connection_change != NULL) {
                transport_state.callbacks.on_connection_change(false);
            }

            /* Restart advertising with error handling */
            ret = hal_ble_start_advertising(NULL);
            if (ret != HAL_BLE_OK) {
                LOG_ERROR("Failed to restart advertising after disconnect: %s (code=%d)",
                          hal_ble_error_to_string(ret), ret);
                /* Try to recover by stopping and restarting */
                LOG_INFO("Attempting to recover advertising...");
                hal_ble_stop_advertising();
                ret = hal_ble_start_advertising(NULL);
                if (ret != HAL_BLE_OK) {
                    LOG_ERROR(
                        "Failed to recover advertising: %s (code=%d) - BLE transport "
                        "unavailable",
                        hal_ble_error_to_string(ret), ret);
                    transport_state.state = BLE_TRANSPORT_STATE_IDLE;
                    led_set_pattern(LED_PATTERN_IDLE);
                } else {
                    LOG_INFO("Advertising recovered successfully");
                }
            }
            break;

        case HAL_BLE_EVENT_WRITE:
            /* Validate event data */
            if (event->data == NULL || event->data_len == 0) {
                LOG_ERROR("Invalid write event: data=%p, len=%zu", (void *) event->data,
                          event->data_len);
                break;
            }

            /* Validate data length to prevent buffer overflow */
            if (event->data_len > BLE_FRAGMENT_MAX_MESSAGE_SIZE) {
                LOG_ERROR("Write data too large: %zu bytes (max %d), rejecting", event->data_len,
                          BLE_FRAGMENT_MAX_MESSAGE_SIZE);
                /* Reset fragment buffer to prevent corruption */
                ble_fragment_reset(&transport_state.rx_fragment);
                break;
            }

            /* Update activity timestamp */
            update_activity_timestamp();

            /* Handle Control Point writes */
            if (event->char_handle == ble_fido_service_get_control_point_handle()) {
                ble_fido_service_on_control_point_write(event->conn_handle, event->data,
                                                        event->data_len);
            } else {
                LOG_WARN("Write to unexpected characteristic handle: %d", event->char_handle);
            }
            break;

        case HAL_BLE_EVENT_NOTIFY_ENABLED:
            LOG_INFO("Notifications enabled on char_handle=%d", event->char_handle);
            if (event->char_handle == ble_fido_service_get_status_handle()) {
                ble_fido_service_set_status_notify_enabled(event->conn_handle, true);
            } else {
                LOG_WARN("Notifications enabled on unexpected characteristic: %d",
                         event->char_handle);
            }
            break;

        case HAL_BLE_EVENT_NOTIFY_DISABLED:
            LOG_INFO("Notifications disabled on char_handle=%d", event->char_handle);
            if (event->char_handle == ble_fido_service_get_status_handle()) {
                ble_fido_service_set_status_notify_enabled(event->conn_handle, false);
            } else {
                LOG_WARN("Notifications disabled on unexpected characteristic: %d",
                         event->char_handle);
            }
            break;

        case HAL_BLE_EVENT_MTU_CHANGED:
            LOG_INFO("MTU changed: %d", event->mtu);
            /* Validate MTU size */
            if (event->mtu < 23) {
                LOG_ERROR("Invalid MTU size: %d (minimum 23), using default", event->mtu);
                transport_state.connection.mtu = BLE_FRAGMENT_DEFAULT_MTU;
            } else {
                transport_state.connection.mtu = event->mtu;
            }
            break;

        case HAL_BLE_EVENT_ENCRYPTION_CHANGED:
            LOG_INFO("Encryption changed: encrypted=%d (conn_handle=%d)", event->encrypted,
                     event->conn_handle);
            transport_state.connection.is_encrypted = event->encrypted;

            /* Check encryption status */
            if (!event->encrypted) {
                LOG_ERROR("Connection encryption disabled - SECURITY VIOLATION (conn_handle=%d)",
                          event->conn_handle);
                /* If we were processing, abort the operation */
                if (transport_state.state == BLE_TRANSPORT_STATE_PROCESSING) {
                    LOG_ERROR("Aborting CTAP operation due to encryption loss");
                    cleanup_operation_state();
                }
                /* Disconnect the insecure connection immediately */
                LOG_ERROR("Disconnecting insecure connection (conn_handle=%d)", event->conn_handle);
                ret = hal_ble_disconnect(event->conn_handle);
                if (ret != HAL_BLE_OK) {
                    LOG_ERROR("Failed to disconnect insecure connection: %s (code=%d)",
                              hal_ble_error_to_string(ret), ret);
                }
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

/* ========== Error Handling Helpers ========== */

/**
 * @brief Clean up operation state after error or completion
 *
 * Ensures all operation-related state is properly cleaned up.
 */
static void cleanup_operation_state(void)
{
    LOG_DEBUG("Cleaning up operation state");

    /* Reset fragment buffer */
    ble_fragment_reset(&transport_state.rx_fragment);

    /* Clear transport busy state */
    if (transport_is_busy()) {
        transport_set_busy(false);
    }

    /* Unlock transport */
    if (transport_is_locked()) {
        transport_unlock();
    }

    /* Return to connected state if we were processing */
    if (transport_state.state == BLE_TRANSPORT_STATE_PROCESSING) {
        transport_state.state = BLE_TRANSPORT_STATE_CONNECTED;

        /* Restore LED pattern to connected (solid on) */
        led_set_pattern(LED_PATTERN_BLE_CONNECTED);
    }
}

/**
 * @brief Send CTAP error response
 *
 * Sends a CTAP error response to the client when an error occurs.
 *
 * @param error_code CTAP error code
 * @return 0 on success, negative error code otherwise
 */
static int send_ctap_error_response(uint8_t error_code)
{
    if (!ble_transport_is_connected()) {
        LOG_ERROR("Cannot send error response: not connected");
        return BLE_TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_INFO("Sending CTAP error response: 0x%02X", error_code);

    /* CTAP error response is just the error code byte */
    uint8_t response[1] = {error_code};

    int ret = ble_transport_send_response(response, sizeof(response));
    if (ret != BLE_TRANSPORT_OK) {
        LOG_ERROR("Failed to send CTAP error response: %s", ble_transport_error_to_string(ret));
        return ret;
    }

    return BLE_TRANSPORT_OK;
}

/**
 * @brief Convert BLE transport error code to string
 *
 * @param error_code BLE transport error code
 * @return Error string
 */
static const char *ble_transport_error_to_string(int error_code)
{
    switch (error_code) {
        case BLE_TRANSPORT_OK:
            return "OK";
        case BLE_TRANSPORT_ERROR:
            return "ERROR";
        case BLE_TRANSPORT_ERROR_INVALID_PARAM:
            return "INVALID_PARAM";
        case BLE_TRANSPORT_ERROR_NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case BLE_TRANSPORT_ERROR_NOT_CONNECTED:
            return "NOT_CONNECTED";
        case BLE_TRANSPORT_ERROR_BUSY:
            return "BUSY";
        case BLE_TRANSPORT_ERROR_NOT_ENCRYPTED:
            return "NOT_ENCRYPTED";
        case BLE_TRANSPORT_ERROR_NOT_PAIRED:
            return "NOT_PAIRED";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert HAL BLE error code to string
 *
 * @param error_code HAL BLE error code
 * @return Error string
 */
static const char *hal_ble_error_to_string(int error_code)
{
    switch (error_code) {
        case HAL_BLE_OK:
            return "OK";
        case HAL_BLE_ERROR:
            return "ERROR";
        case HAL_BLE_ERROR_NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case HAL_BLE_ERROR_BUSY:
            return "BUSY";
        case HAL_BLE_ERROR_TIMEOUT:
            return "TIMEOUT";
        case HAL_BLE_ERROR_NO_MEM:
            return "NO_MEM";
        default:
            return "UNKNOWN";
    }
}

/* ========== Transport Abstraction Integration ========== */

/**
 * @brief BLE transport send wrapper for transport abstraction
 *
 * Wraps ble_transport_send_response() to match transport_ops_t signature.
 *
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
static int ble_transport_send_wrapper(const uint8_t *data, size_t len)
{
    int ret = ble_transport_send_response(data, len);
    if (ret == BLE_TRANSPORT_OK) {
        return (int) len;
    }
    return ret;
}

int ble_transport_receive_wrapper(uint8_t *data, size_t max_len, uint8_t *cmd)
{
    /* BLE transport uses a callback model, not polling */
    /* Return 0 to indicate no data available (non-blocking) */
    (void) data;
    (void) max_len;
    (void) cmd;
    return 0;
}

/**
 * @brief Get BLE transport name
 *
 * @return Transport name string
 */
static const char *ble_transport_get_name(void)
{
    return "BLE-GATT";
}

int ble_transport_register(void)
{
    if (!transport_state.initialized) {
        LOG_ERROR("BLE transport not initialized, cannot register");
        return BLE_TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    LOG_INFO("Registering BLE transport with transport abstraction");

    transport_ops_t ops = {.send = ble_transport_send_wrapper,
                           .receive = ble_transport_receive_wrapper,
                           .is_connected = ble_transport_is_connected,
                           .get_name = ble_transport_get_name};

    int ret = transport_register(TRANSPORT_TYPE_BLE, &ops);
    if (ret != TRANSPORT_OK) {
        LOG_ERROR("Failed to register BLE transport: %d", ret);
        return BLE_TRANSPORT_ERROR;
    }

    LOG_INFO("BLE transport registered with transport abstraction");

    return BLE_TRANSPORT_OK;
}
