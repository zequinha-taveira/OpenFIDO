/**
 * @file buffer.h
 * @brief Safe Buffer Operations
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Securely zero memory
 * 
 * @param ptr Pointer to memory
 * @param len Length to zero
 */
void secure_zero(void *ptr, size_t len);

/**
 * @brief Constant-time memory comparison
 * 
 * @param a First buffer
 * @param b Second buffer
 * @param len Length to compare
 * @return 0 if equal, non-zero otherwise
 */
int constant_time_compare(const void *a, const void *b, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H */
