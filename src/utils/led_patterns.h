/**
 * @file led_patterns.h
 * @brief LED Pattern Customization
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef LED_PATTERNS_H
#define LED_PATTERNS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LED Pattern Types */
typedef enum {
    LED_PATTERN_IDLE = 0,      /* Device idle */
    LED_PATTERN_PROCESSING,    /* Processing request */
    LED_PATTERN_USER_PRESENCE, /* Waiting for user presence */
    LED_PATTERN_SUCCESS,       /* Operation successful */
    LED_PATTERN_ERROR,         /* Operation failed */
    LED_PATTERN_BOOTLOADER,    /* Bootloader/rescue mode */
    LED_PATTERN_BLE_ADVERTISING, /* BLE advertising (slow blink) */
    LED_PATTERN_BLE_CONNECTED,   /* BLE connected (solid on) */
    LED_PATTERN_BLE_PROCESSING,  /* BLE processing (fast blink) */
    LED_PATTERN_CUSTOM         /* User-defined pattern */
} led_pattern_type_t;

/* LED Pattern Definition */
typedef struct {
    uint16_t on_ms;       /* LED on duration in milliseconds */
    uint16_t off_ms;      /* LED off duration in milliseconds */
    uint8_t repeat_count; /* Number of times to repeat (0 = infinite) */
    uint8_t brightness;   /* Brightness level (0-255) */
} led_pattern_t;

/**
 * @brief Initialize LED pattern system
 */
void led_patterns_init(void);

/**
 * @brief Set active LED pattern
 *
 * @param type Pattern type
 */
void led_set_pattern(led_pattern_type_t type);

/**
 * @brief Set custom LED pattern
 *
 * @param pattern Custom pattern definition
 */
void led_set_custom_pattern(const led_pattern_t *pattern);

/**
 * @brief Get current pattern
 *
 * @return Current pattern type
 */
led_pattern_type_t led_get_current_pattern(void);

/**
 * @brief Update LED state (call from main loop)
 */
void led_patterns_update(void);

/**
 * @brief Load LED patterns from storage
 */
void led_patterns_load(void);

/**
 * @brief Save LED patterns to storage
 */
void led_patterns_save(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_PATTERNS_H */
