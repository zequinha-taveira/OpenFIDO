/**
 * @file usb_keyboard.h
 * @brief USB HID Keyboard Emulation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef USB_KEYBOARD_H
#define USB_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize USB keyboard interface
 *
 * @return 0 on success, error code otherwise
 */
int usb_keyboard_init(void);

/**
 * @brief Type a string via USB keyboard
 *
 * @param text Text to type
 * @return 0 on success, error code otherwise
 */
int usb_keyboard_type_string(const char *text);

/**
 * @brief Type OTP code via USB keyboard
 *
 * @param code OTP code to type
 * @param add_enter Add Enter key after code
 * @return 0 on success, error code otherwise
 */
int usb_keyboard_type_otp(uint32_t code, bool add_enter);

/**
 * @brief Send key press
 *
 * @param keycode HID keycode
 * @param modifiers Modifier keys (Ctrl, Alt, etc.)
 * @return 0 on success, error code otherwise
 */
int usb_keyboard_send_key(uint8_t keycode, uint8_t modifiers);

#ifdef __cplusplus
}
#endif

#endif /* USB_KEYBOARD_H */
