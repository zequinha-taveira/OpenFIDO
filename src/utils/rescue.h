/**
 * @file rescue.h
 * @brief Rescue/Recovery Mode Interface
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef RESCUE_H
#define RESCUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Rescue mode commands */
typedef enum {
    RESCUE_CMD_GET_INFO = 0x01,
    RESCUE_CMD_FACTORY_RESET = 0x02,
    RESCUE_CMD_FIRMWARE_UPDATE = 0x03,
    RESCUE_CMD_DIAGNOSTICS = 0x04,
    RESCUE_CMD_EXIT = 0xFF
} rescue_command_t;

/**
 * @brief Check if device should enter rescue mode
 *
 * @return true if rescue mode should be entered
 */
bool rescue_should_enter(void);

/**
 * @brief Enter rescue mode
 *
 * This function does not return until rescue mode is exited
 */
void rescue_enter(void);

/**
 * @brief Process rescue mode command
 *
 * @param cmd Command to process
 * @param data Command data
 * @param data_len Length of command data
 * @param response Response buffer
 * @param response_len Response length
 * @return 0 on success, error code otherwise
 */
int rescue_process_command(rescue_command_t cmd, const uint8_t *data, size_t data_len,
                           uint8_t *response, size_t *response_len);

/**
 * @brief Perform factory reset
 *
 * @return 0 on success, error code otherwise
 */
int rescue_factory_reset(void);

/**
 * @brief Get device diagnostics
 *
 * @param buffer Output buffer for diagnostics
 * @param buffer_size Size of output buffer
 * @return Number of bytes written
 */
size_t rescue_get_diagnostics(uint8_t *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* RESCUE_H */
