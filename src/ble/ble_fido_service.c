/**
 * @file ble_fido_service.c
 * @brief BLE FIDO GATT Service Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "ble_fido_service.h"
#include "../hal/hal_ble.h"
#include "../utils/logger.h"
#include <string.h>

/* ========== Service State ========== */

typedef struct {
    bool initialized;
    uint16_t service_handle;
    uint16_t control_point_handle;
    uint16_t status_handle;
    uint16_t control_point_length_handle;
    uint16_t service_revision_handle;
    uint16_t service_revision_bitfield_handle;
    bool status_notify_enabled;
    ble_fido_service_callbacks_t callbacks;
} ble_fido_service_state_t;

static ble_fido_service_state_t service_state = {
    .initialized = false,
    .service_handle = 0,
    .control_point_handle = 0,
    .status_handle = 0,
    .control_point_length_handle = 0,
    .service_revision_handle = 0,
    .service_revision_bitfield_handle = 0,
    .status_notify_enabled = false
};

/* ========== Service Configuration ========== */

/* Maximum Control Point write length (512 bytes) */
#define CONTROL_POINT_MAX_LENGTH 512

/* Service revision: FIDO2 1.0 */
static const uint8_t service_revision = BLE_FIDO_SERVICE_REVISION_1_0;

/* Service revision bitfield: Support FIDO2 1.0 */
static const uint8_t service_revision_bitfield = BLE_FIDO_SERVICE_REVISION_1_0;

/* Control Point length value (big-endian) */
static const uint8_t control_point_length[2] = {
    (CONTROL_POINT_MAX_LENGTH >> 8) & 0xFF,
    CONTROL_POINT_MAX_LENGTH & 0xFF
};

/* ========== Service Initialization ========== */

int ble_fido_service_init(const ble_fido_service_callbacks_t *callbacks) {
    if (callbacks == NULL) {
        return BLE_FIDO_SERVICE_ERROR_INVALID_PARAM;
    }

    if (service_state.initialized) {
        LOG_WARN("FIDO service already initialized");
        return BLE_FIDO_SERVICE_OK;
    }

    LOG_INFO("Initializing FIDO service");

    /* Store callbacks */
    service_state.callbacks = *callbacks;

    /* Register FIDO service */
    int ret = hal_ble_gatt_register_service(BLE_FIDO_SERVICE_UUID, 
                                             &service_state.service_handle);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to register FIDO service: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    LOG_INFO("FIDO service registered: handle=%d", service_state.service_handle);

    /* Add Control Point characteristic (Write) */
    ret = hal_ble_gatt_add_characteristic(
        service_state.service_handle,
        BLE_FIDO_CONTROL_POINT_UUID,
        HAL_BLE_GATT_PROP_WRITE,
        CONTROL_POINT_MAX_LENGTH,
        &service_state.control_point_handle
    );
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to add Control Point characteristic: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    LOG_INFO("Control Point characteristic added: handle=%d", service_state.control_point_handle);

    /* Add Status characteristic (Notify) */
    ret = hal_ble_gatt_add_characteristic(
        service_state.service_handle,
        BLE_FIDO_STATUS_UUID,
        HAL_BLE_GATT_PROP_NOTIFY,
        CONTROL_POINT_MAX_LENGTH,
        &service_state.status_handle
    );
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to add Status characteristic: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    LOG_INFO("Status characteristic added: handle=%d", service_state.status_handle);

    /* Add Control Point Length characteristic (Read) */
    ret = hal_ble_gatt_add_characteristic(
        service_state.service_handle,
        BLE_FIDO_CONTROL_POINT_LENGTH_UUID,
        HAL_BLE_GATT_PROP_READ,
        2,
        &service_state.control_point_length_handle
    );
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to add Control Point Length characteristic: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    /* Set Control Point Length value */
    hal_ble_gatt_set_value(service_state.control_point_length_handle, 
                           control_point_length, 2);

    LOG_INFO("Control Point Length characteristic added: handle=%d", 
             service_state.control_point_length_handle);

    /* Add Service Revision characteristic (Read) */
    ret = hal_ble_gatt_add_characteristic(
        service_state.service_handle,
        BLE_FIDO_SERVICE_REVISION_UUID,
        HAL_BLE_GATT_PROP_READ,
        1,
        &service_state.service_revision_handle
    );
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to add Service Revision characteristic: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    /* Set Service Revision value */
    hal_ble_gatt_set_value(service_state.service_revision_handle, 
                           &service_revision, 1);

    LOG_INFO("Service Revision characteristic added: handle=%d", 
             service_state.service_revision_handle);

    /* Add Service Revision Bitfield characteristic (Read) */
    ret = hal_ble_gatt_add_characteristic(
        service_state.service_handle,
        BLE_FIDO_SERVICE_REVISION_BITFIELD_UUID,
        HAL_BLE_GATT_PROP_READ,
        1,
        &service_state.service_revision_bitfield_handle
    );
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to add Service Revision Bitfield characteristic: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    /* Set Service Revision Bitfield value */
    hal_ble_gatt_set_value(service_state.service_revision_bitfield_handle, 
                           &service_revision_bitfield, 1);

    LOG_INFO("Service Revision Bitfield characteristic added: handle=%d", 
             service_state.service_revision_bitfield_handle);

    service_state.initialized = true;
    LOG_INFO("FIDO service initialization complete");

    return BLE_FIDO_SERVICE_OK;
}

/* ========== Access Control ========== */

bool ble_fido_service_is_authorized(uint16_t conn_handle) {
    /* Check if connection is encrypted (requires pairing) */
    bool encrypted = false;
    int ret = hal_ble_is_encrypted(conn_handle, &encrypted);
    
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to check encryption status: %d", ret);
        return false;
    }

    if (!encrypted) {
        LOG_WARN("Rejecting unpaired access attempt on conn=%d", conn_handle);
        return false;
    }

    return true;
}

/* ========== Control Point Handler ========== */

void ble_fido_service_on_control_point_write(uint16_t conn_handle,
                                              const uint8_t *data, 
                                              size_t len) {
    if (!service_state.initialized) {
        LOG_ERROR("FIDO service not initialized");
        return;
    }

    if (data == NULL || len == 0) {
        LOG_ERROR("Invalid Control Point write: data=%p, len=%zu", data, len);
        return;
    }

    /* Enforce pairing requirement */
    if (!ble_fido_service_is_authorized(conn_handle)) {
        LOG_ERROR("Unauthorized Control Point write attempt on conn=%d", conn_handle);
        return;
    }

    LOG_DEBUG("Control Point write: conn=%d, len=%zu", conn_handle, len);

    /* Call registered callback */
    if (service_state.callbacks.on_control_point_write != NULL) {
        service_state.callbacks.on_control_point_write(conn_handle, data, len);
    }
}

/* ========== Status Notification ========== */

int ble_fido_service_send_status(uint16_t conn_handle,
                                  const uint8_t *data, 
                                  size_t len) {
    if (!service_state.initialized) {
        LOG_ERROR("FIDO service not initialized");
        return BLE_FIDO_SERVICE_ERROR_NOT_INITIALIZED;
    }

    if (data == NULL || len == 0) {
        LOG_ERROR("Invalid Status data: data=%p, len=%zu", data, len);
        return BLE_FIDO_SERVICE_ERROR_INVALID_PARAM;
    }

    /* Enforce pairing requirement */
    if (!ble_fido_service_is_authorized(conn_handle)) {
        LOG_ERROR("Unauthorized Status notification attempt on conn=%d", conn_handle);
        return BLE_FIDO_SERVICE_ERROR_NOT_PAIRED;
    }

    if (!service_state.status_notify_enabled) {
        LOG_WARN("Status notifications not enabled");
        return BLE_FIDO_SERVICE_ERROR;
    }

    LOG_DEBUG("Sending Status notification: conn=%d, len=%zu", conn_handle, len);

    /* Send notification via BLE HAL */
    int ret = hal_ble_notify(conn_handle, service_state.status_handle, data, len);
    if (ret != HAL_BLE_OK) {
        LOG_ERROR("Failed to send Status notification: %d", ret);
        return BLE_FIDO_SERVICE_ERROR;
    }

    return BLE_FIDO_SERVICE_OK;
}

/* ========== Getters ========== */

uint16_t ble_fido_service_get_control_point_handle(void) {
    return service_state.control_point_handle;
}

uint16_t ble_fido_service_get_status_handle(void) {
    return service_state.status_handle;
}

bool ble_fido_service_is_status_notify_enabled(uint16_t conn_handle) {
    (void)conn_handle; /* Currently tracking globally, not per-connection */
    return service_state.status_notify_enabled;
}

/* ========== Notification State Management ========== */

/**
 * @brief Set Status notification state
 * 
 * Called by BLE HAL when client enables/disables notifications.
 * 
 * @param conn_handle Connection handle
 * @param enabled true if enabled, false if disabled
 */
void ble_fido_service_set_status_notify_enabled(uint16_t conn_handle, bool enabled) {
    if (!service_state.initialized) {
        return;
    }

    service_state.status_notify_enabled = enabled;

    LOG_INFO("Status notifications %s for conn=%d", 
             enabled ? "enabled" : "disabled", conn_handle);

    /* Call registered callback */
    if (service_state.callbacks.on_status_notify != NULL) {
        service_state.callbacks.on_status_notify(conn_handle, enabled);
    }
}
