/**
 * @file transport.c
 * @brief Transport Abstraction Layer Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "transport.h"

#include <string.h>

#include "../utils/logger.h"

/* ========== Transport State ========== */

/**
 * @brief Transport registration entry
 */
typedef struct {
    bool registered;
    transport_ops_t ops;
} transport_entry_t;

/**
 * @brief Transport abstraction state
 */
typedef struct {
    bool initialized;
    transport_entry_t transports[TRANSPORT_TYPE_MAX];
    transport_type_t active_transport;
    bool operation_in_progress;
    bool locked;
    transport_type_t locked_transport;
} transport_state_t;

static transport_state_t state = {
    .initialized = false,
    .active_transport = TRANSPORT_TYPE_MAX,
    .operation_in_progress = false,
    .locked = false,
    .locked_transport = TRANSPORT_TYPE_MAX
};

/* ========== Transport Management ========== */

int transport_init(void)
{
    if (state.initialized) {
        LOG_WARN("Transport abstraction already initialized");
        return TRANSPORT_OK;
    }

    LOG_INFO("Initializing transport abstraction layer");

    /* Clear all transport entries */
    memset(&state, 0, sizeof(state));
    state.active_transport = TRANSPORT_TYPE_MAX;
    state.locked_transport = TRANSPORT_TYPE_MAX;

    state.initialized = true;

    LOG_INFO("Transport abstraction initialized");

    return TRANSPORT_OK;
}

int transport_register(transport_type_t type, const transport_ops_t *ops)
{
    if (!state.initialized) {
        LOG_ERROR("Transport not initialized");
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        LOG_ERROR("Invalid transport type: %d", type);
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (ops == NULL) {
        LOG_ERROR("Transport ops cannot be NULL");
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (ops->send == NULL || ops->receive == NULL || ops->is_connected == NULL) {
        LOG_ERROR("Transport ops missing required functions");
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (state.transports[type].registered) {
        LOG_ERROR("Transport %s already registered", transport_type_name(type));
        return TRANSPORT_ERROR_ALREADY_REGISTERED;
    }

    LOG_INFO("Registering transport: %s", transport_type_name(type));

    state.transports[type].registered = true;
    state.transports[type].ops = *ops;

    LOG_INFO("Transport %s registered successfully", transport_type_name(type));

    return TRANSPORT_OK;
}

int transport_unregister(transport_type_t type)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!state.transports[type].registered) {
        return TRANSPORT_ERROR_NOT_REGISTERED;
    }

    LOG_INFO("Unregistering transport: %s", transport_type_name(type));

    /* Clear active transport if this was it */
    if (state.active_transport == type) {
        state.active_transport = TRANSPORT_TYPE_MAX;
    }

    /* Clear locked transport if this was it */
    if (state.locked_transport == type) {
        state.locked = false;
        state.locked_transport = TRANSPORT_TYPE_MAX;
    }

    state.transports[type].registered = false;
    memset(&state.transports[type].ops, 0, sizeof(transport_ops_t));

    return TRANSPORT_OK;
}

bool transport_is_registered(transport_type_t type)
{
    if (!state.initialized || type >= TRANSPORT_TYPE_MAX) {
        return false;
    }

    return state.transports[type].registered;
}

/* ========== Transport Selection ========== */

int transport_set_active(transport_type_t type)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!state.transports[type].registered) {
        LOG_ERROR("Cannot set active: transport %s not registered", transport_type_name(type));
        return TRANSPORT_ERROR_NOT_REGISTERED;
    }

    /* Check if transport is locked to a different transport */
    if (state.locked && state.locked_transport != type) {
        LOG_WARN("Cannot set active: transport locked to %s",
                 transport_type_name(state.locked_transport));
        return TRANSPORT_ERROR_LOCKED;
    }

    LOG_DEBUG("Setting active transport: %s", transport_type_name(type));

    state.active_transport = type;

    return TRANSPORT_OK;
}

transport_type_t transport_get_active(void)
{
    return state.active_transport;
}

bool transport_is_active(transport_type_t type)
{
    if (!state.initialized || type >= TRANSPORT_TYPE_MAX) {
        return false;
    }

    return state.active_transport == type;
}

/* ========== Transport Operations ========== */

int transport_send(const uint8_t *data, size_t len)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (state.active_transport >= TRANSPORT_TYPE_MAX) {
        LOG_ERROR("No active transport for send");
        return TRANSPORT_ERROR_NO_ACTIVE_TRANSPORT;
    }

    return transport_send_on(state.active_transport, data, len);
}

int transport_send_on(transport_type_t type, const uint8_t *data, size_t len)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (data == NULL || len == 0) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!state.transports[type].registered) {
        LOG_ERROR("Transport %s not registered", transport_type_name(type));
        return TRANSPORT_ERROR_NOT_REGISTERED;
    }

    if (!state.transports[type].ops.is_connected()) {
        LOG_ERROR("Transport %s not connected", transport_type_name(type));
        return TRANSPORT_ERROR_NOT_CONNECTED;
    }

    LOG_DEBUG("Sending %zu bytes on %s", len, transport_type_name(type));

    int ret = state.transports[type].ops.send(data, len);
    if (ret < 0) {
        LOG_ERROR("Send failed on %s: %d", transport_type_name(type), ret);
        return ret;
    }

    LOG_DEBUG("Sent %d bytes on %s", ret, transport_type_name(type));

    return ret;
}

int transport_receive_from(transport_type_t type, uint8_t *data, size_t max_len, uint8_t *cmd)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (data == NULL || max_len == 0) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!state.transports[type].registered) {
        return TRANSPORT_ERROR_NOT_REGISTERED;
    }

    int ret = state.transports[type].ops.receive(data, max_len, cmd);
    if (ret < 0) {
        /* Don't log timeout errors as they're expected */
        if (ret != -2) { /* USB_HID_ERROR_TIMEOUT */
            LOG_DEBUG("Receive failed on %s: %d", transport_type_name(type), ret);
        }
        return ret;
    }

    if (ret > 0) {
        LOG_DEBUG("Received %d bytes on %s", ret, transport_type_name(type));
    }

    return ret;
}

bool transport_any_connected(void)
{
    if (!state.initialized) {
        return false;
    }

    for (int i = 0; i < TRANSPORT_TYPE_MAX; i++) {
        if (state.transports[i].registered && state.transports[i].ops.is_connected()) {
            return true;
        }
    }

    return false;
}

bool transport_is_connected(transport_type_t type)
{
    if (!state.initialized || type >= TRANSPORT_TYPE_MAX) {
        return false;
    }

    if (!state.transports[type].registered) {
        return false;
    }

    return state.transports[type].ops.is_connected();
}

/* ========== Operation State Management ========== */

bool transport_is_busy(void)
{
    return state.operation_in_progress;
}

void transport_set_busy(bool busy)
{
    state.operation_in_progress = busy;
}

int transport_lock(transport_type_t type)
{
    if (!state.initialized) {
        return TRANSPORT_ERROR_NOT_INITIALIZED;
    }

    if (type >= TRANSPORT_TYPE_MAX) {
        return TRANSPORT_ERROR_INVALID_PARAM;
    }

    if (!state.transports[type].registered) {
        return TRANSPORT_ERROR_NOT_REGISTERED;
    }

    if (state.locked && state.locked_transport != type) {
        LOG_WARN("Cannot lock %s: already locked to %s",
                 transport_type_name(type),
                 transport_type_name(state.locked_transport));
        return TRANSPORT_ERROR_LOCKED;
    }

    LOG_DEBUG("Locking transport: %s", transport_type_name(type));

    state.locked = true;
    state.locked_transport = type;

    return TRANSPORT_OK;
}

void transport_unlock(void)
{
    if (state.locked) {
        LOG_DEBUG("Unlocking transport: %s", transport_type_name(state.locked_transport));
        state.locked = false;
        state.locked_transport = TRANSPORT_TYPE_MAX;
    }
}

bool transport_is_locked(void)
{
    return state.locked;
}

transport_type_t transport_get_locked(void)
{
    return state.locked_transport;
}

/* ========== Utility Functions ========== */

const char *transport_type_name(transport_type_t type)
{
    switch (type) {
        case TRANSPORT_TYPE_USB:
            return "USB";
        case TRANSPORT_TYPE_BLE:
            return "BLE";
        default:
            return "UNKNOWN";
    }
}
