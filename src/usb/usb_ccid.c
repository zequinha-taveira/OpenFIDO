/**
 * @file usb_ccid.c
 * @brief USB CCID (Chip Card Interface Device) Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "usb_ccid.h"

#include <string.h>

#include "hal.h"
#include "logger.h"

/* Maximum number of registered applications */
#define MAX_APPS 4

/* CCID State */
typedef struct {
    bool card_present;
    uint8_t slot_status;
    uint8_t sequence;
    uint8_t atr[ATR_MAX_LENGTH];
    size_t atr_len;
    uint8_t selected_app;
} ccid_state_t;

/* Application Registry */
typedef struct {
    uint8_t aid[16];
    size_t aid_len;
    apdu_handler_t handler;
    bool active;
} app_entry_t;

static ccid_state_t ccid_state;
static app_entry_t app_registry[MAX_APPS];

/* Default ATR for OpenFIDO */
static const uint8_t default_atr[] = {
    0x3B, 0x8E, 0x80, 0x01, /* TS, T0, TA1, TB1 */
    0x80, 0x31, 0xC0, 0x64, /* TC1, TD1, TA2, TB2 */
    0x77, 0xE3, 0x03, 0x00, /* TC2, TD2, TA3, Historical bytes start */
    0x00, 0x82, 0x90, 0x00  /* Historical bytes + TCK */
};

/**
 * @brief Build CCID response header
 */
static void build_response_header(uint8_t *response, uint8_t msg_type, uint32_t length,
                                  uint8_t slot, uint8_t seq, uint8_t status, uint8_t error)
{
    ccid_header_t *hdr = (ccid_header_t *) response;
    hdr->bMessageType = msg_type;
    hdr->dwLength = length;
    hdr->bSlot = slot;
    hdr->bSeq = seq;
    hdr->bSpecific[0] = status;
    hdr->bSpecific[1] = error;
    hdr->bSpecific[2] = 0;
}

/**
 * @brief Handle ICC Power On command
 */
static int handle_icc_power_on(const uint8_t *data, size_t len, uint8_t *response, size_t *resp_len)
{
    const ccid_header_t *hdr = (const ccid_header_t *) data;

    LOG_INFO("CCID: ICC Power On");

    /* Build response with ATR */
    build_response_header(response, CCID_RDR_TO_PC_DATABLOCK, ccid_state.atr_len, hdr->bSlot,
                          hdr->bSeq, CCID_CMD_STATUS_OK, 0);

    memcpy(response + CCID_HEADER_SIZE, ccid_state.atr, ccid_state.atr_len);
    *resp_len = CCID_HEADER_SIZE + ccid_state.atr_len;

    ccid_state.slot_status = CCID_ICC_PRESENT_ACTIVE;

    return 0;
}

/**
 * @brief Handle ICC Power Off command
 */
static int handle_icc_power_off(const uint8_t *data, size_t len, uint8_t *response,
                                size_t *resp_len)
{
    const ccid_header_t *hdr = (const ccid_header_t *) data;

    LOG_INFO("CCID: ICC Power Off");

    build_response_header(response, CCID_RDR_TO_PC_SLOTSTATUS, 0, hdr->bSlot, hdr->bSeq,
                          CCID_CMD_STATUS_OK, 0);

    *resp_len = CCID_HEADER_SIZE;

    ccid_state.slot_status = CCID_ICC_PRESENT_INACTIVE;
    ccid_state.selected_app = 0xFF;

    return 0;
}

/**
 * @brief Handle Get Slot Status command
 */
static int handle_get_slot_status(const uint8_t *data, size_t len, uint8_t *response,
                                  size_t *resp_len)
{
    const ccid_header_t *hdr = (const ccid_header_t *) data;

    build_response_header(response, CCID_RDR_TO_PC_SLOTSTATUS, 0, hdr->bSlot, hdr->bSeq,
                          CCID_CMD_STATUS_OK, 0);

    *resp_len = CCID_HEADER_SIZE;

    return 0;
}

/**
 * @brief Parse APDU command
 */
static int parse_apdu(const uint8_t *data, size_t len, apdu_command_t *cmd)
{
    if (len < 4) {
        return -1;
    }

    cmd->cla = data[0];
    cmd->ins = data[1];
    cmd->p1 = data[2];
    cmd->p2 = data[3];

    if (len == 4) {
        /* Case 1: No data */
        cmd->lc = 0;
        cmd->le = 0;
    } else if (len == 5) {
        /* Case 2: Le only */
        cmd->lc = 0;
        cmd->le = data[4];
    } else {
        /* Case 3/4: Lc and data */
        cmd->lc = data[4];
        if (len < 5 + cmd->lc) {
            return -1;
        }
        memcpy(cmd->data, &data[5], cmd->lc);

        if (len > 5 + cmd->lc) {
            cmd->le = data[5 + cmd->lc];
        } else {
            cmd->le = 0;
        }
    }

    return 0;
}

/**
 * @brief Handle SELECT command (find and activate application)
 */
static int handle_select(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* SELECT by AID */
    if (cmd->p1 == 0x04) {
        for (int i = 0; i < MAX_APPS; i++) {
            if (app_registry[i].active && app_registry[i].aid_len == cmd->lc &&
                memcmp(app_registry[i].aid, cmd->data, cmd->lc) == 0) {
                LOG_INFO("CCID: Selected application %d", i);
                ccid_state.selected_app = i;

                /* Success */
                resp->len = 0;
                resp->sw1 = 0x90;
                resp->sw2 = 0x00;
                return 0;
            }
        }

        /* Application not found */
        resp->len = 0;
        resp->sw1 = 0x6A;
        resp->sw2 = 0x82;
        return 0;
    }

    /* Unsupported SELECT type */
    resp->len = 0;
    resp->sw1 = 0x6A;
    resp->sw2 = 0x86;
    return 0;
}

/**
 * @brief Process APDU command
 */
static int process_apdu(const apdu_command_t *cmd, apdu_response_t *resp)
{
    /* Handle SELECT command */
    if (cmd->ins == 0xA4) {
        return handle_select(cmd, resp);
    }

    /* Route to selected application */
    if (ccid_state.selected_app < MAX_APPS && app_registry[ccid_state.selected_app].active) {
        return app_registry[ccid_state.selected_app].handler(cmd, resp);
    }

    /* No application selected */
    resp->len = 0;
    resp->sw1 = 0x6D;
    resp->sw2 = 0x00;
    return 0;
}

/**
 * @brief Handle XFR Block command (APDU exchange)
 */
static int handle_xfr_block(const uint8_t *data, size_t len, uint8_t *response, size_t *resp_len)
{
    const ccid_header_t *hdr = (const ccid_header_t *) data;
    apdu_command_t cmd;
    apdu_response_t resp;

    /* Parse APDU */
    if (parse_apdu(data + CCID_HEADER_SIZE, hdr->dwLength, &cmd) != 0) {
        LOG_ERROR("CCID: Invalid APDU");
        build_response_header(response, CCID_RDR_TO_PC_DATABLOCK, 0, hdr->bSlot, hdr->bSeq,
                              CCID_CMD_STATUS_FAILED, CCID_ERROR_BAD_ATR_TS);
        *resp_len = CCID_HEADER_SIZE;
        return -1;
    }

    LOG_DEBUG("CCID: APDU CLA=%02X INS=%02X P1=%02X P2=%02X Lc=%d", cmd.cla, cmd.ins, cmd.p1,
              cmd.p2, cmd.lc);

    /* Process APDU */
    memset(&resp, 0, sizeof(resp));
    process_apdu(&cmd, &resp);

    /* Build response */
    uint16_t response_len = resp.len + 2; /* data + SW1 + SW2 */
    build_response_header(response, CCID_RDR_TO_PC_DATABLOCK, response_len, hdr->bSlot, hdr->bSeq,
                          CCID_CMD_STATUS_OK, 0);

    memcpy(response + CCID_HEADER_SIZE, resp.data, resp.len);
    response[CCID_HEADER_SIZE + resp.len] = resp.sw1;
    response[CCID_HEADER_SIZE + resp.len + 1] = resp.sw2;

    *resp_len = CCID_HEADER_SIZE + response_len;

    LOG_DEBUG("CCID: Response SW=%02X%02X len=%d", resp.sw1, resp.sw2, resp.len);

    return 0;
}

int usb_ccid_init(void)
{
    LOG_INFO("Initializing CCID interface");

    memset(&ccid_state, 0, sizeof(ccid_state));
    memset(app_registry, 0, sizeof(app_registry));

    /* Set default ATR */
    memcpy(ccid_state.atr, default_atr, sizeof(default_atr));
    ccid_state.atr_len = sizeof(default_atr);

    /* Card is present by default */
    ccid_state.card_present = true;
    ccid_state.slot_status = CCID_ICC_PRESENT_INACTIVE;
    ccid_state.selected_app = 0xFF;

    LOG_INFO("CCID initialized successfully");
    return 0;
}

int usb_ccid_process_message(const uint8_t *data, size_t len, uint8_t *response, size_t *resp_len)
{
    if (len < CCID_HEADER_SIZE) {
        LOG_ERROR("CCID: Message too short");
        return -1;
    }

    const ccid_header_t *hdr = (const ccid_header_t *) data;

    LOG_DEBUG("CCID: Message type 0x%02X, slot %d, seq %d, len %u", hdr->bMessageType, hdr->bSlot,
              hdr->bSeq, hdr->dwLength);

    ccid_state.sequence = hdr->bSeq;

    switch (hdr->bMessageType) {
        case CCID_PC_TO_RDR_ICCPOWERON:
            return handle_icc_power_on(data, len, response, resp_len);

        case CCID_PC_TO_RDR_ICCPOWEROFF:
            return handle_icc_power_off(data, len, response, resp_len);

        case CCID_PC_TO_RDR_GETSLOTSTATUS:
            return handle_get_slot_status(data, len, response, resp_len);

        case CCID_PC_TO_RDR_XFRBLOCK:
            return handle_xfr_block(data, len, response, resp_len);

        default:
            LOG_WARN("CCID: Unsupported message type 0x%02X", hdr->bMessageType);
            build_response_header(response, CCID_RDR_TO_PC_SLOTSTATUS, 0, hdr->bSlot, hdr->bSeq,
                                  CCID_CMD_STATUS_FAILED, CCID_ERROR_CMD_NOT_SUPPORTED);
            *resp_len = CCID_HEADER_SIZE;
            return -1;
    }
}

int usb_ccid_register_app(const uint8_t *aid, size_t aid_len, apdu_handler_t handler)
{
    if (aid_len > 16 || handler == NULL) {
        return -1;
    }

    /* Find free slot */
    for (int i = 0; i < MAX_APPS; i++) {
        if (!app_registry[i].active) {
            memcpy(app_registry[i].aid, aid, aid_len);
            app_registry[i].aid_len = aid_len;
            app_registry[i].handler = handler;
            app_registry[i].active = true;

            LOG_INFO("CCID: Registered application %d (AID len=%zu)", i, aid_len);
            return 0;
        }
    }

    LOG_ERROR("CCID: No free application slots");
    return -1;
}

int usb_ccid_get_atr(uint8_t *atr, size_t *len)
{
    if (atr == NULL || len == NULL) {
        return -1;
    }

    memcpy(atr, ccid_state.atr, ccid_state.atr_len);
    *len = ccid_state.atr_len;

    return 0;
}

void usb_ccid_set_card_present(bool present)
{
    ccid_state.card_present = present;
    if (present) {
        ccid_state.slot_status = CCID_ICC_PRESENT_INACTIVE;
    } else {
        ccid_state.slot_status = CCID_ICC_NOT_PRESENT;
    }
}

uint8_t usb_ccid_get_slot_status(void)
{
    return ccid_state.slot_status;
}
