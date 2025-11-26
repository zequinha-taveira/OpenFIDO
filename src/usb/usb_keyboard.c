/**
 * @file usb_keyboard.c
 * @brief USB HID Keyboard Emulation Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#include "usb_keyboard.h"

#include <stdio.h>
#include <string.h>

#include "hal.h"
#include "logger.h"

/* HID Keycodes */
#define HID_KEY_ENTER 0x28
#define HID_KEY_0 0x27
#define HID_KEY_1 0x1E
#define HID_KEY_2 0x1F
#define HID_KEY_3 0x20
#define HID_KEY_4 0x21
#define HID_KEY_5 0x22
#define HID_KEY_6 0x23
#define HID_KEY_7 0x24
#define HID_KEY_8 0x25
#define HID_KEY_9 0x26

/* Keycode lookup for digits */
static const uint8_t digit_keycodes[] = {HID_KEY_0, HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_4,
                                         HID_KEY_5, HID_KEY_6, HID_KEY_7, HID_KEY_8, HID_KEY_9};

int usb_keyboard_init(void)
{
    /* Initialize USB keyboard interface */
    /* This would configure USB composite device with HID keyboard */
    LOG_INFO("USB keyboard initialized");
    return 0;
}

int usb_keyboard_send_key(uint8_t keycode, uint8_t modifiers)
{
    /* Send HID keyboard report */
    /* Format: [modifiers, reserved, key1, key2, key3, key4, key5, key6] */
    uint8_t report[8] = {modifiers, 0, keycode, 0, 0, 0, 0, 0};

    /* Send key press */
    /* This would use platform-specific USB HID send function */
    LOG_DEBUG("Keyboard: keycode=0x%02X, modifiers=0x%02X", keycode, modifiers);

    /* Small delay */
    hal_delay_ms(10);

    /* Send key release */
    uint8_t release[8] = {0};
    /* Send release report */

    hal_delay_ms(10);

    return 0;
}

int usb_keyboard_type_string(const char *text)
{
    /* Type each character */
    for (size_t i = 0; text[i] != '\0'; i++) {
        /* Convert character to HID keycode */
        /* This is simplified - full implementation would handle all characters */
        uint8_t keycode = 0;

        if (text[i] >= '0' && text[i] <= '9') {
            keycode = digit_keycodes[text[i] - '0'];
        } else {
            /* Skip unsupported characters for now */
            continue;
        }

        usb_keyboard_send_key(keycode, 0);
    }

    return 0;
}

int usb_keyboard_type_otp(uint32_t code, bool add_enter)
{
    /* Convert code to string */
    char code_str[16];
    snprintf(code_str, sizeof(code_str), "%u", code);

    LOG_INFO("Typing OTP: %s", code_str);

    /* Type the code */
    usb_keyboard_type_string(code_str);

    /* Add Enter key if requested */
    if (add_enter) {
        usb_keyboard_send_key(HID_KEY_ENTER, 0);
    }

    return 0;
}
