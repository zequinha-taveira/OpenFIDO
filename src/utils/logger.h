/**
 * @file logger.h
 * @brief Logging Utilities
 * 
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log Levels */
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} log_level_t;

/* Current log level (can be configured) */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

/* Initialize logger */
void logger_init(void);

/* Logging macros */
#define LOG_ERROR(fmt, ...) \
    do { if (LOG_LEVEL >= LOG_LEVEL_ERROR) printf("[ERROR] " fmt "\n", ##__VA_ARGS__); } while(0)

#define LOG_WARN(fmt, ...) \
    do { if (LOG_LEVEL >= LOG_LEVEL_WARN) printf("[WARN]  " fmt "\n", ##__VA_ARGS__); } while(0)

#define LOG_INFO(fmt, ...) \
    do { if (LOG_LEVEL >= LOG_LEVEL_INFO) printf("[INFO]  " fmt "\n", ##__VA_ARGS__); } while(0)

#define LOG_DEBUG(fmt, ...) \
    do { if (LOG_LEVEL >= LOG_LEVEL_DEBUG) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
