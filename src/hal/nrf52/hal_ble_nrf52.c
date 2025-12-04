/**
 * @file hal_ble_nrf52.c
 * @brief Nordic nRF52 BLE Hardware Abstraction Layer Implementation
 *
 * Implements BLE HAL using Nordic SoftDevice BLE stack (S132/S140).
 * Provides FIDO GATT service, advertising, pairing, and encryption.
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "../hal_ble.h"
#include "../../utils/logger.h"

#ifdef NRF52

#include <string.h>
#include "ble.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_hci.h"
#include "nrf_ble_gatt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"

/* BLE Configuration */
#define APP_BLE_CONN_CFG_TAG 1
#define APP_BLE_OBSERVER_PRIO 3
#define MIN_CONN_INTERVAL MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define SLAVE_LATENCY 0
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(4000, UNIT_10_MS)

/* Advertising Configuration */
#define APP_ADV_INTERVAL 300
#define APP_ADV_DURATION 18000
#define DEVICE_NAME "OpenFIDO"

/* GATT Service Handles */
typedef struct {
    uint16_t service_handle;
    uint16_t char_handles[8];
    uint8_t char_count;
} gatt_service_t;

/* BLE State */
static struct {
    bool initialized;
    hal_ble_event_callback_t event_callback;
    uint16_t conn_handle;
    gatt_service_t services[4];
    uint8_t service_count;
    bool advertising;
    nrf_ble_gatt_t gatt_module;
    ble_advertising_t adv_module;
} ble_state = {
    .conn_handle = BLE_CONN_HANDLE_INVALID,
    .initialized = false,
    .advertising = false,
    .service_count = 0
};

/* Forward declarations */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);
static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt);
static void advertising_evt_handler(ble_adv_evt_t ble_adv_evt);
static void conn_params_evt_handler(ble_conn_params_evt_t *p_evt);
static void pm_evt_handler(pm_evt_t const *p_evt);

/* ========== Helper Functions ========== */

static void send_event(hal_ble_event_type_t type) {
    if (ble_state.event_callback) {
        hal_ble_event_t event = {
            .type = type,
            .conn_handle = ble_state.conn_handle,
            .char_handle = 0,
            .data = NULL,
            .data_len = 0,
            .mtu = 0,
            .encrypted = false,
            .error_code = 0
        };
        ble_state.event_callback(&event);
    }
}

/* ========== Initialization and Control ========== */

int hal_ble_init(hal_ble_event_callback_t callback) {
    if (ble_state.initialized) {
        return HAL_BLE_OK;
    }

    if (callback == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Initializing nRF52 BLE HAL");

    ble_state.event_callback = callback;

    /* Initialize SoftDevice */
    ret_code_t err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("SoftDevice enable failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Configure BLE stack */
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("BLE config failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Enable BLE stack */
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("BLE enable failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Register BLE event handler */
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

    /* Initialize GATT module */
    err_code = nrf_ble_gatt_init(&ble_state.gatt_module, gatt_evt_handler);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("GATT init failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Initialize Peer Manager for pairing */
    err_code = pm_init();
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Peer Manager init failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Configure security */
    ble_gap_sec_params_t sec_params = {
        .bond = 1,
        .mitm = 1,
        .lesc = 1,
        .keypress = 0,
        .io_caps = BLE_GAP_IO_CAPS_DISPLAY_YESNO,
        .oob = 0,
        .min_key_size = 7,
        .max_key_size = 16,
        .kdist_own = {.enc = 1, .id = 1},
        .kdist_peer = {.enc = 1, .id = 1}
    };

    err_code = pm_sec_params_set(&sec_params);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Security params failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Register PM event handler */
    err_code = pm_register(pm_evt_handler);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("PM register failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Set device name */
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Device name set failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    ble_state.initialized = true;
    LOG_INFO("nRF52 BLE HAL initialized");

    return HAL_BLE_OK;
}

int hal_ble_deinit(void) {
    if (!ble_state.initialized) {
        return HAL_BLE_OK;
    }

    hal_ble_stop_advertising();
    
    if (ble_state.conn_handle != BLE_CONN_HANDLE_INVALID) {
        sd_ble_gap_disconnect(ble_state.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    }

    nrf_sdh_disable_request();
    ble_state.initialized = false;

    return HAL_BLE_OK;
}

bool hal_ble_is_supported(void) {
    return true;
}

/* ========== Advertising ========== */

int hal_ble_start_advertising(const hal_ble_adv_params_t *params) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ret_code_t err_code;

    /* Set advertising data */
    ble_advdata_t advdata = {0};
    ble_advdata_t srdata = {0};

    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    /* Add FIDO service UUID to advertising data */
    ble_uuid_t adv_uuids[] = {{0xFFFD, BLE_UUID_TYPE_BLE}};
    srdata.uuids_complete.uuid_cnt = 1;
    srdata.uuids_complete.p_uuids = adv_uuids;

    /* Configure advertising parameters */
    ble_gap_adv_params_t adv_params = {0};
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr = NULL;
    adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = (params && params->interval_min_ms) ? 
                          MSEC_TO_UNITS(params->interval_min_ms, UNIT_0_625_MS) : APP_ADV_INTERVAL;
    adv_params.duration = (params && params->timeout_s) ? params->timeout_s * 100 : APP_ADV_DURATION;
    adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

    /* Encode advertising data */
    uint8_t adv_data_buf[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
    ble_gap_adv_data_t gap_adv_data = {
        .adv_data = {.p_data = adv_data_buf, .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX},
        .scan_rsp_data = {.p_data = NULL, .len = 0}
    };

    err_code = ble_advdata_encode(&advdata, gap_adv_data.adv_data.p_data, &gap_adv_data.adv_data.len);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Advertising data encode failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    /* Start advertising */
    uint8_t adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
    err_code = sd_ble_gap_adv_set_configure(&adv_handle, &gap_adv_data, &adv_params);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Advertising configure failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    err_code = sd_ble_gap_adv_start(adv_handle, APP_BLE_CONN_CFG_TAG);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Advertising start failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    ble_state.advertising = true;
    LOG_INFO("BLE advertising started");

    return HAL_BLE_OK;
}

int hal_ble_stop_advertising(void) {
    if (!ble_state.advertising) {
        return HAL_BLE_OK;
    }

    ret_code_t err_code = sd_ble_gap_adv_stop(BLE_GAP_ADV_SET_HANDLE_NOT_SET);
    if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE) {
        LOG_ERROR("Advertising stop failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    ble_state.advertising = false;
    LOG_INFO("BLE advertising stopped");

    return HAL_BLE_OK;
}

bool hal_ble_is_advertising(void) {
    return ble_state.advertising;
}

/* ========== GATT Operations ========== */

int hal_ble_gatt_register_service(uint16_t service_uuid, uint16_t *service_handle) {
    if (!ble_state.initialized || service_handle == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    if (ble_state.service_count >= 4) {
        return HAL_BLE_ERROR_NO_MEM;
    }

    ble_uuid_t ble_uuid = {
        .uuid = service_uuid,
        .type = BLE_UUID_TYPE_BLE
    };

    ret_code_t err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, 
                                                     &ble_uuid, 
                                                     &ble_state.services[ble_state.service_count].service_handle);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Service add failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    *service_handle = ble_state.services[ble_state.service_count].service_handle;
    ble_state.services[ble_state.service_count].char_count = 0;
    ble_state.service_count++;

    LOG_INFO("GATT service registered: UUID=0x%04X, handle=%d", service_uuid, *service_handle);

    return HAL_BLE_OK;
}

int hal_ble_gatt_add_characteristic(uint16_t service_handle, uint32_t char_uuid,
                                     uint8_t properties, uint16_t max_len,
                                     uint16_t *char_handle) {
    if (!ble_state.initialized || char_handle == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    /* Find service */
    gatt_service_t *service = NULL;
    for (uint8_t i = 0; i < ble_state.service_count; i++) {
        if (ble_state.services[i].service_handle == service_handle) {
            service = &ble_state.services[i];
            break;
        }
    }

    if (service == NULL || service->char_count >= 8) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    /* Configure characteristic */
    ble_gatts_char_md_t char_md = {0};
    ble_gatts_attr_md_t cccd_md = {0};
    ble_gatts_attr_t attr_char_value = {0};
    ble_uuid_t ble_uuid = {0};
    ble_gatts_attr_md_t attr_md = {0};

    /* Set characteristic properties */
    if (properties & HAL_BLE_GATT_PROP_READ) {
        char_md.char_props.read = 1;
    }
    if (properties & HAL_BLE_GATT_PROP_WRITE) {
        char_md.char_props.write = 1;
    }
    if (properties & HAL_BLE_GATT_PROP_WRITE_NO_RSP) {
        char_md.char_props.write_wo_resp = 1;
    }
    if (properties & HAL_BLE_GATT_PROP_NOTIFY) {
        char_md.char_props.notify = 1;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
    }

    /* Set UUID */
    ble_uuid.type = BLE_UUID_TYPE_VENDOR_BEGIN;
    ble_uuid.uuid = (uint16_t)(char_uuid & 0xFFFF);

    /* Add vendor-specific UUID if needed */
    ble_uuid128_t base_uuid = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
    memcpy(&base_uuid[12], &char_uuid, 4);
    sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type);

    /* Set attribute metadata */
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 1;

    /* Set attribute value */
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = max_len;

    /* Add characteristic */
    ble_gatts_char_handles_t handles;
    ret_code_t err_code = sd_ble_gatts_characteristic_add(service_handle, &char_md, 
                                                            &attr_char_value, &handles);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Characteristic add failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    *char_handle = handles.value_handle;
    service->char_handles[service->char_count++] = handles.value_handle;

    LOG_INFO("GATT characteristic added: UUID=0x%08X, handle=%d", char_uuid, *char_handle);

    return HAL_BLE_OK;
}

int hal_ble_notify(hal_ble_conn_handle_t conn_handle, uint16_t char_handle,
                   const uint8_t *data, size_t len) {
    if (!ble_state.initialized || data == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    if (conn_handle == HAL_BLE_CONN_HANDLE_INVALID) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ble_gatts_hvx_params_t hvx_params = {0};
    uint16_t hvx_len = len;

    hvx_params.handle = char_handle;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len = &hvx_len;
    hvx_params.p_data = data;

    ret_code_t err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Notification send failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

int hal_ble_gatt_set_value(uint16_t char_handle, const uint8_t *data, size_t len) {
    if (!ble_state.initialized || data == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    ble_gatts_value_t gatts_value = {0};
    gatts_value.len = len;
    gatts_value.offset = 0;
    gatts_value.p_value = (uint8_t *)data;

    ret_code_t err_code = sd_ble_gatts_value_set(ble_state.conn_handle, char_handle, &gatts_value);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("GATT value set failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

/* ========== Connection Management ========== */

int hal_ble_disconnect(hal_ble_conn_handle_t conn_handle) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ret_code_t err_code = sd_ble_gap_disconnect(conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Disconnect failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

int hal_ble_get_mtu(hal_ble_conn_handle_t conn_handle, uint16_t *mtu) {
    if (!ble_state.initialized || mtu == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    *mtu = nrf_ble_gatt_eff_mtu_get(&ble_state.gatt_module, conn_handle);
    return HAL_BLE_OK;
}

int hal_ble_is_encrypted(hal_ble_conn_handle_t conn_handle, bool *encrypted) {
    if (!ble_state.initialized || encrypted == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    ble_gap_conn_sec_t conn_sec;
    ret_code_t err_code = sd_ble_gap_conn_sec_get(conn_handle, &conn_sec);
    if (err_code != NRF_SUCCESS) {
        return HAL_BLE_ERROR;
    }

    *encrypted = (conn_sec.sec_mode.sm > 0 && conn_sec.sec_mode.lv > 0);
    return HAL_BLE_OK;
}

int hal_ble_update_connection_params(hal_ble_conn_handle_t conn_handle,
                                      uint16_t interval_min_ms, uint16_t interval_max_ms,
                                      uint16_t latency, uint16_t timeout_ms) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ble_gap_conn_params_t conn_params = {0};
    conn_params.min_conn_interval = MSEC_TO_UNITS(interval_min_ms, UNIT_1_25_MS);
    conn_params.max_conn_interval = MSEC_TO_UNITS(interval_max_ms, UNIT_1_25_MS);
    conn_params.slave_latency = latency;
    conn_params.conn_sup_timeout = MSEC_TO_UNITS(timeout_ms, UNIT_10_MS);

    ret_code_t err_code = sd_ble_gap_conn_param_update(conn_handle, &conn_params);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Connection param update failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

/* ========== Security Operations ========== */

int hal_ble_set_security_config(const hal_ble_security_config_t *config) {
    if (!ble_state.initialized || config == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    /* Security configuration is set during initialization */
    /* This function could be extended to update security settings dynamically */
    
    return HAL_BLE_OK;
}

int hal_ble_start_pairing(hal_ble_conn_handle_t conn_handle) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ret_code_t err_code = pm_conn_secure(conn_handle, false);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Pairing start failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

int hal_ble_pairing_confirm(hal_ble_conn_handle_t conn_handle, bool confirm) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    ret_code_t err_code = sd_ble_gap_auth_key_reply(conn_handle, 
                                                      confirm ? BLE_GAP_AUTH_KEY_TYPE_PASSKEY : BLE_GAP_AUTH_KEY_TYPE_NONE,
                                                      NULL);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Pairing confirm failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

int hal_ble_delete_bond(hal_ble_conn_handle_t conn_handle) {
    if (!ble_state.initialized) {
        return HAL_BLE_ERROR_INVALID_STATE;
    }

    pm_peer_id_t peer_id;
    ret_code_t err_code = pm_peer_id_get(conn_handle, &peer_id);
    if (err_code != NRF_SUCCESS) {
        return HAL_BLE_ERROR;
    }

    err_code = pm_peer_delete(peer_id);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Bond delete failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

/* ========== Device Information ========== */

int hal_ble_set_device_name(const char *name) {
    if (!ble_state.initialized || name == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    ret_code_t err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)name, strlen(name));
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Device name set failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    return HAL_BLE_OK;
}

int hal_ble_get_address(uint8_t addr[6]) {
    if (!ble_state.initialized || addr == NULL) {
        return HAL_BLE_ERROR_INVALID_PARAM;
    }

    ble_gap_addr_t gap_addr;
    ret_code_t err_code = sd_ble_gap_addr_get(&gap_addr);
    if (err_code != NRF_SUCCESS) {
        LOG_ERROR("Address get failed: %d", err_code);
        return HAL_BLE_ERROR;
    }

    memcpy(addr, gap_addr.addr, 6);
    return HAL_BLE_OK;
}

/* ========== Event Handlers ========== */

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
    ret_code_t err_code;

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            LOG_INFO("BLE connected");
            ble_state.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            ble_state.advertising = false;
            send_event(HAL_BLE_EVENT_CONNECTED);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            LOG_INFO("BLE disconnected");
            ble_state.conn_handle = BLE_CONN_HANDLE_INVALID;
            send_event(HAL_BLE_EVENT_DISCONNECTED);
            /* Restart advertising */
            hal_ble_start_advertising(NULL);
            break;

        case BLE_GATTS_EVT_WRITE: {
            const ble_gatts_evt_write_t *p_write = &p_ble_evt->evt.gatts_evt.params.write;
            if (ble_state.event_callback) {
                hal_ble_event_t event = {
                    .type = HAL_BLE_EVENT_WRITE,
                    .conn_handle = p_ble_evt->evt.gatts_evt.conn_handle,
                    .char_handle = p_write->handle,
                    .data = p_write->data,
                    .data_len = p_write->len,
                    .mtu = 0,
                    .encrypted = false,
                    .error_code = 0
                };
                ble_state.event_callback(&event);
            }
            break;
        }

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            send_event(HAL_BLE_EVENT_NOTIFY_COMPLETE);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            /* Pairing request - handled by Peer Manager */
            err_code = pm_conn_secure(p_ble_evt->evt.gap_evt.conn_handle, false);
            if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_BUSY) {
                LOG_ERROR("Security request failed: %d", err_code);
            }
            break;

        case BLE_GAP_EVT_CONN_SEC_UPDATE: {
            const ble_gap_evt_conn_sec_update_t *p_sec = &p_ble_evt->evt.gap_evt.params.conn_sec_update;
            if (ble_state.event_callback) {
                hal_ble_event_t event = {
                    .type = HAL_BLE_EVENT_ENCRYPTION_CHANGED,
                    .conn_handle = p_ble_evt->evt.gap_evt.conn_handle,
                    .char_handle = 0,
                    .data = NULL,
                    .data_len = 0,
                    .mtu = 0,
                    .encrypted = (p_sec->conn_sec.sec_mode.sm > 0 && p_sec->conn_sec.sec_mode.lv > 0),
                    .error_code = 0
                };
                ble_state.event_callback(&event);
            }
            break;
        }

        default:
            break;
    }
}

static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt) {
    if (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED) {
        LOG_INFO("MTU updated to %d", p_evt->params.att_mtu_effective);
        if (ble_state.event_callback) {
            hal_ble_event_t event = {
                .type = HAL_BLE_EVENT_MTU_CHANGED,
                .conn_handle = p_evt->conn_handle,
                .char_handle = 0,
                .data = NULL,
                .data_len = 0,
                .mtu = p_evt->params.att_mtu_effective,
                .encrypted = false,
                .error_code = 0
            };
            ble_state.event_callback(&event);
        }
    }
}

static void advertising_evt_handler(ble_adv_evt_t ble_adv_evt) {
    switch (ble_adv_evt) {
        case BLE_ADV_EVT_FAST:
            LOG_INFO("Fast advertising started");
            break;

        case BLE_ADV_EVT_IDLE:
            LOG_INFO("Advertising idle");
            ble_state.advertising = false;
            break;

        default:
            break;
    }
}

static void conn_params_evt_handler(ble_conn_params_evt_t *p_evt) {
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
        LOG_ERROR("Connection parameters negotiation failed");
    }
}

static void pm_evt_handler(pm_evt_t const *p_evt) {
    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id) {
        case PM_EVT_BONDED_PEER_CONNECTED:
            LOG_INFO("Bonded peer connected");
            break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
            LOG_INFO("Pairing succeeded");
            send_event(HAL_BLE_EVENT_PAIRING_COMPLETE);
            break;

        case PM_EVT_CONN_SEC_FAILED:
            LOG_ERROR("Pairing failed");
            if (ble_state.event_callback) {
                hal_ble_event_t event = {
                    .type = HAL_BLE_EVENT_PAIRING_FAILED,
                    .conn_handle = p_evt->conn_handle,
                    .char_handle = 0,
                    .data = NULL,
                    .data_len = 0,
                    .mtu = 0,
                    .encrypted = false,
                    .error_code = p_evt->params.conn_sec_failed.error
                };
                ble_state.event_callback(&event);
            }
            break;

        default:
            break;
    }
}

#endif /* NRF52 */
