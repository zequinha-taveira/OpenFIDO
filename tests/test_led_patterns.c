/**
 * @file test_led_patterns.c
 * @brief Unit tests for LED patterns
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "led_patterns.h"

/* Test helper macros */
#define TEST_ASSERT(condition)                                            \
    do {                                                                  \
        if (!(condition)) {                                               \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

#define TEST_PASS()                     \
    do {                                \
        printf("PASS: %s\n", __func__); \
        return 0;                       \
    } while (0)

/* Test BLE LED patterns are defined */
int test_ble_led_patterns_defined(void)
{
    /* Initialize LED patterns */
    led_patterns_init();

    /* Test BLE advertising pattern */
    led_set_pattern(LED_PATTERN_BLE_ADVERTISING);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_ADVERTISING);

    /* Test BLE connected pattern */
    led_set_pattern(LED_PATTERN_BLE_CONNECTED);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_CONNECTED);

    /* Test BLE processing pattern */
    led_set_pattern(LED_PATTERN_BLE_PROCESSING);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_PROCESSING);

    TEST_PASS();
}

/* Test pattern transitions */
int test_ble_pattern_transitions(void)
{
    led_patterns_init();

    /* Simulate BLE state transitions */
    
    /* Start advertising */
    led_set_pattern(LED_PATTERN_BLE_ADVERTISING);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_ADVERTISING);

    /* Connect */
    led_set_pattern(LED_PATTERN_BLE_CONNECTED);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_CONNECTED);

    /* Process request */
    led_set_pattern(LED_PATTERN_BLE_PROCESSING);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_PROCESSING);

    /* Return to connected */
    led_set_pattern(LED_PATTERN_BLE_CONNECTED);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_CONNECTED);

    /* Disconnect */
    led_set_pattern(LED_PATTERN_BLE_ADVERTISING);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_BLE_ADVERTISING);

    /* Stop */
    led_set_pattern(LED_PATTERN_IDLE);
    TEST_ASSERT(led_get_current_pattern() == LED_PATTERN_IDLE);

    TEST_PASS();
}

/* Main test runner */
int main(int argc, char *argv[])
{
    int result = 0;

    printf("Running LED pattern tests...\n");

    result |= test_ble_led_patterns_defined();
    result |= test_ble_pattern_transitions();

    if (result == 0) {
        printf("\nAll LED pattern tests passed!\n");
    } else {
        printf("\nSome LED pattern tests failed!\n");
    }

    return result;
}
