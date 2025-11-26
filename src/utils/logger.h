/**
 * @file logger.h
 * @brief Logging Utilities
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
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

/* Initialize logger */
void logger_init(void);

/* Set log level at runtime */
void logger_set_level(log_level_t level);

/* Get current log level */
log_level_t logger_get_level(void);

/* Core logging function */
void logger_log(log_level_t level, const char *file, int line, const char *fmt, ...);

/* Logging macros with context */
#define LOG_ERROR(fmt, ...) logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) logger_log(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* Context logging macros (for future use) */
#define LOG_ERROR_CTX(ctx, fmt, ...) \
    logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, "[%s] " fmt, ctx, ##__VA_ARGS__)

#define LOG_WARN_CTX(ctx, fmt, ...) \
    logger_log(LOG_LEVEL_WARN, __FILE__, __LINE__, "[%s] " fmt, ctx, ##__VA_ARGS__)

#define LOG_INFO_CTX(ctx, fmt, ...) \
    logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, "[%s] " fmt, ctx, ##__VA_ARGS__)

#define LOG_DEBUG_CTX(ctx, fmt, ...) \
    logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "[%s] " fmt, ctx, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H */
