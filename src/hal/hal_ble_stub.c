/**
 * @file hal_ble_stub.c
 * @brief Stub BLE HAL implementation for platforms without BLE support
 *
 * This file provides stub implementations of the BLE HAL interface that
 * return HAL_BLE_ERROR_NOT_SUPPORTED. This allows the firmware to compile
 * and run on platforms without BLE hardware, gracefully degrading to
 * USB-only operation.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "hal_ble.h"

/* ========== Initialization and Control ========== */

int hal_ble_init(hal_ble_event_callback_t callback)
{
    (void) callback;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_deinit(void)
{
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

bool hal_ble_is_supported(void)
{
    return false;
}

/* ========== Advertising ========== */

int hal_ble_start_advertising(const hal_ble_adv_params_t *params)
{
    (void) params;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_stop_advertising(void)
{
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

bool hal_ble_is_advertising(void)
{
    return false;
}

/* ========== GATT Operations ========== */

int hal_ble_gatt_register_service(uint16_t service_uuid, uint16_t *service_handle)
{
    (void) service_uuid;
    (void) service_handle;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_gatt_add_characteristic(uint16_t service_handle, uint32_t char_uuid, uint8_t properties,
                                    uint16_t max_len, uint16_t *char_handle)
{
    (void) service_handle;
    (void) char_uuid;
    (void) properties;
    (void) max_len;
    (void) char_handle;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_notify(hal_ble_conn_handle_t conn_handle, uint16_t char_handle, const uint8_t *data,
                   size_t len)
{
    (void) conn_handle;
    (void) char_handle;
    (void) data;
    (void) len;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_gatt_set_value(uint16_t char_handle, const uint8_t *data, size_t len)
{
    (void) char_handle;
    (void) data;
    (void) len;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

/* ========== Connection Management ========== */

int hal_ble_disconnect(hal_ble_conn_handle_t conn_handle)
{
    (void) conn_handle;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_get_mtu(hal_ble_conn_handle_t conn_handle, uint16_t *mtu)
{
    (void) conn_handle;
    (void) mtu;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_is_encrypted(hal_ble_conn_handle_t conn_handle, bool *encrypted)
{
    (void) conn_handle;
    (void) encrypted;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_update_connection_params(hal_ble_conn_handle_t conn_handle, uint16_t interval_min_ms,
                                     uint16_t interval_max_ms, uint16_t latency,
                                     uint16_t timeout_ms)
{
    (void) conn_handle;
    (void) interval_min_ms;
    (void) interval_max_ms;
    (void) latency;
    (void) timeout_ms;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

/* ========== Security Operations ========== */

int hal_ble_set_security_config(const hal_ble_security_config_t *config)
{
    (void) config;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_start_pairing(hal_ble_conn_handle_t conn_handle)
{
    (void) conn_handle;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_pairing_confirm(hal_ble_conn_handle_t conn_handle, bool confirm)
{
    (void) conn_handle;
    (void) confirm;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_delete_bond(hal_ble_conn_handle_t conn_handle)
{
    (void) conn_handle;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

/* ========== Device Information ========== */

int hal_ble_set_device_name(const char *name)
{
    (void) name;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}

int hal_ble_get_address(uint8_t addr[6])
{
    (void) addr;
    return HAL_BLE_ERROR_NOT_SUPPORTED;
}
