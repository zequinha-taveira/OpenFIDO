/**
 * @file types.h
 * @brief Common type definitions for OpenFIDO
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Common result codes */
typedef enum {
    OPENFIDO_OK = 0,
    OPENFIDO_ERROR = -1,
    OPENFIDO_ERROR_INVALID_PARAM = -2,
    OPENFIDO_ERROR_NO_MEMORY = -3,
    OPENFIDO_ERROR_NOT_FOUND = -4,
    OPENFIDO_ERROR_TIMEOUT = -5,
    OPENFIDO_ERROR_NOT_SUPPORTED = -6,
    OPENFIDO_ERROR_BUSY = -7,
    OPENFIDO_ERROR_IO = -8,
    OPENFIDO_ERROR_SECURITY = -9,
} openfido_result_t;

/* Common buffer structure */
typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
} openfido_buffer_t;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_TYPES_H */
