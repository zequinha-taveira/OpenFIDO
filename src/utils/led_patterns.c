/**
 * @file led_patterns.c
 * @brief LED Pattern Customization Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "led_patterns.h"

#include <string.h>

#include "hal.h"
#include "logger.h"

/* Default LED patterns */
static const led_pattern_t default_patterns[] = {
    [LED_PATTERN_IDLE] = {0, 0, 0, 0},              /* Off */
    [LED_PATTERN_PROCESSING] = {100, 100, 0, 128},  /* Blink medium */
    [LED_PATTERN_USER_PRESENCE] = {50, 50, 0, 255}, /* Blink fast */
    [LED_PATTERN_SUCCESS] = {500, 0, 1, 255},       /* Solid 500ms */
    [LED_PATTERN_ERROR] = {100, 100, 3, 255},       /* Blink 3 times */
    [LED_PATTERN_BOOTLOADER] = {200, 800, 0, 128},  /* Slow blink */
    [LED_PATTERN_CUSTOM] = {0, 0, 0, 0}             /* User-defined */
};

/* Current state */
static struct {
    led_pattern_type_t current_type;
    led_pattern_t current_pattern;
    led_pattern_t custom_pattern;
    uint32_t last_update_ms;
    uint8_t repeat_counter;
    bool led_state;
} led_state = {0};

void led_patterns_init(void)
{
    memset(&led_state, 0, sizeof(led_state));
    led_state.current_type = LED_PATTERN_IDLE;

    /* Load custom patterns from storage if available */
    led_patterns_load();

    LOG_INFO("LED patterns initialized");
}

void led_set_pattern(led_pattern_type_t type)
{
    if (type >= LED_PATTERN_CUSTOM) {
        LOG_WARN("Invalid LED pattern type: %d", type);
        return;
    }

    led_state.current_type = type;

    if (type == LED_PATTERN_CUSTOM) {
        memcpy(&led_state.current_pattern, &led_state.custom_pattern, sizeof(led_pattern_t));
    } else {
        memcpy(&led_state.current_pattern, &default_patterns[type], sizeof(led_pattern_t));
    }

    led_state.repeat_counter = 0;
    led_state.led_state = false;
    led_state.last_update_ms = hal_get_time_ms();

    /* Turn off LED initially */
    hal_led_set_state(HAL_LED_OFF);

    LOG_DEBUG("LED pattern set to: %d", type);
}

void led_set_custom_pattern(const led_pattern_t *pattern)
{
    if (pattern == NULL) {
        return;
    }

    memcpy(&led_state.custom_pattern, pattern, sizeof(led_pattern_t));

    /* If custom pattern is active, update current pattern */
    if (led_state.current_type == LED_PATTERN_CUSTOM) {
        memcpy(&led_state.current_pattern, pattern, sizeof(led_pattern_t));
        led_state.repeat_counter = 0;
        led_state.last_update_ms = hal_get_time_ms();
    }

    LOG_INFO("Custom LED pattern set: on=%dms, off=%dms, repeat=%d", pattern->on_ms,
             pattern->off_ms, pattern->repeat_count);
}

led_pattern_type_t led_get_current_pattern(void)
{
    return led_state.current_type;
}

void led_patterns_update(void)
{
    uint32_t now_ms = hal_get_time_ms();
    led_pattern_t *pattern = &led_state.current_pattern;

    /* Check if pattern is finished */
    if (pattern->repeat_count > 0 && led_state.repeat_counter >= pattern->repeat_count) {
        /* Pattern finished, turn off LED */
        hal_led_set_state(HAL_LED_OFF);
        led_state.current_type = LED_PATTERN_IDLE;
        return;
    }

    /* Calculate time since last update */
    uint32_t elapsed_ms = now_ms - led_state.last_update_ms;

    /* Check if it's time to toggle LED */
    uint16_t threshold_ms = led_state.led_state ? pattern->on_ms : pattern->off_ms;

    if (elapsed_ms >= threshold_ms) {
        /* Toggle LED state */
        led_state.led_state = !led_state.led_state;
        led_state.last_update_ms = now_ms;

        if (led_state.led_state) {
            /* Turn on LED */
            hal_led_set_state(HAL_LED_ON);

            /* Increment repeat counter when starting a new cycle */
            if (pattern->repeat_count > 0) {
                led_state.repeat_counter++;
            }
        } else {
            /* Turn off LED */
            hal_led_set_state(HAL_LED_OFF);
        }
    }
}

void led_patterns_load(void)
{
    /* Load custom patterns from storage */
    /* This would read from persistent storage */
    LOG_DEBUG("LED patterns loaded from storage");
}

void led_patterns_save(void)
{
    /* Save custom patterns to storage */
    /* This would write to persistent storage */
    LOG_DEBUG("LED patterns saved to storage");
}
