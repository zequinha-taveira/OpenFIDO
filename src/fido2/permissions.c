/**
 * @file permissions.c
 * @brief CTAP2 Permissions System Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "permissions.h"

#include <string.h>

#include "logger.h"

static permission_state_t perm_state = {0};

void permissions_init(void)
{
    memset(&perm_state, 0, sizeof(perm_state));
    LOG_INFO("Permissions system initialized");
}

bool permissions_set(uint8_t permissions, const uint8_t *rp_id_hash)
{
    perm_state.permissions = permissions;

    if (rp_id_hash != NULL) {
        memcpy(perm_state.rp_id_hash, rp_id_hash, 32);
        perm_state.has_rp_id = true;
    } else {
        perm_state.has_rp_id = false;
    }

    LOG_INFO("Permissions set: 0x%02X (RP ID: %s)", permissions,
             perm_state.has_rp_id ? "yes" : "no");

    return true;
}

bool permissions_check(uint8_t permission, const uint8_t *rp_id_hash)
{
    /* Check if permission bit is set */
    if ((perm_state.permissions & permission) == 0) {
        LOG_WARN("Permission denied: 0x%02X not in 0x%02X", permission, perm_state.permissions);
        return false;
    }

    /* For MC/GA permissions, verify RP ID matches */
    if ((permission == PERM_MAKE_CREDENTIAL || permission == PERM_GET_ASSERTION) &&
        perm_state.has_rp_id) {
        if (rp_id_hash == NULL) {
            LOG_WARN("Permission denied: RP ID required but not provided");
            return false;
        }
        if (memcmp(perm_state.rp_id_hash, rp_id_hash, 32) != 0) {
            LOG_WARN("Permission denied: RP ID mismatch");
            return false;
        }
    }

    return true;
}

void permissions_clear(void)
{
    memset(&perm_state, 0, sizeof(perm_state));
    LOG_INFO("Permissions cleared");
}

const permission_state_t *permissions_get_state(void)
{
    return &perm_state;
}
