/**
 * @file logger.c
 * @brief Logging Utilities Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "logger.h"

#include <stdarg.h>
#include <string.h>

static log_level_t current_level = LOG_LEVEL_INFO;

void logger_init(void)
{
    /* Platform-specific logger initialization can go here */
    /* For now, just use standard printf */
}

void logger_set_level(log_level_t level)
{
    current_level = level;
}

log_level_t logger_get_level(void)
{
    return current_level;
}

void logger_log(log_level_t level, const char *file, int line, const char *fmt, ...)
{
    if (level > current_level) {
        return;
    }

    const char *level_str;
    switch (level) {
        case LOG_LEVEL_ERROR:
            level_str = "ERROR";
            break;
        case LOG_LEVEL_WARN:
            level_str = "WARN ";
            break;
        case LOG_LEVEL_INFO:
            level_str = "INFO ";
            break;
        case LOG_LEVEL_DEBUG:
            level_str = "DEBUG";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }

    /* Extract filename from path */
    const char *filename = strrchr(file, '/');
    if (filename) {
        filename++; /* Skip slash */
    } else {
        filename = strrchr(file, '\\'); /* Try Windows separator */
        if (filename) {
            filename++;
        } else {
            filename = file;
        }
    }

    /* Print log prefix */
    printf("[%s] %s:%d: ", level_str, filename, line);

    /* Print message */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    /* Print newline */
    printf("\n");
}
