// kbmouse.h - Gamepad to Keyboard/Mouse conversion
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith
//
// Converts gamepad input to keyboard and mouse HID reports.
// Enables using any controller for desktop applications, accessibility, or games.

#ifndef KBMOUSE_H
#define KBMOUSE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/services/profiles/profile.h"
#include "descriptors/kbmouse_descriptors.h"

// ============================================================================
// REPORT STRUCTURES
// ============================================================================

// Keyboard report (matches HID descriptor with report ID 1)
typedef struct __attribute__((packed)) {
    uint8_t modifier;       // Modifier keys (Ctrl, Shift, Alt, GUI)
    uint8_t reserved;       // Reserved byte
    uint8_t keycode[6];     // Up to 6 simultaneous keycodes
} kbmouse_keyboard_report_t;

// Mouse report (matches HID descriptor with report ID 2)
typedef struct __attribute__((packed)) {
    uint8_t buttons;        // Button states (5 buttons)
    int8_t x;               // X movement (-127 to 127)
    int8_t y;               // Y movement (-127 to 127)
    int8_t wheel;           // Vertical scroll (-127 to 127)
    int8_t pan;             // Horizontal scroll (-127 to 127)
} kbmouse_mouse_report_t;

// ============================================================================
// KEYBOARD MODIFIERS
// ============================================================================

#define KBMOUSE_MOD_LCTRL   (1 << 0)
#define KBMOUSE_MOD_LSHIFT  (1 << 1)
#define KBMOUSE_MOD_LALT    (1 << 2)
#define KBMOUSE_MOD_LGUI    (1 << 3)
#define KBMOUSE_MOD_RCTRL   (1 << 4)
#define KBMOUSE_MOD_RSHIFT  (1 << 5)
#define KBMOUSE_MOD_RALT    (1 << 6)
#define KBMOUSE_MOD_RGUI    (1 << 7)

// ============================================================================
// HID KEYCODES (USB HID Usage Tables)
// ============================================================================

#define HID_KEY_NONE        0x00
#define HID_KEY_A           0x04
#define HID_KEY_B           0x05
#define HID_KEY_C           0x06
#define HID_KEY_D           0x07
#define HID_KEY_E           0x08
#define HID_KEY_F           0x09
#define HID_KEY_G           0x0A
#define HID_KEY_H           0x0B
#define HID_KEY_I           0x0C
#define HID_KEY_J           0x0D
#define HID_KEY_K           0x0E
#define HID_KEY_L           0x0F
#define HID_KEY_M           0x10
#define HID_KEY_N           0x11
#define HID_KEY_O           0x12
#define HID_KEY_P           0x13
#define HID_KEY_Q           0x14
#define HID_KEY_R           0x15
#define HID_KEY_S           0x16
#define HID_KEY_T           0x17
#define HID_KEY_U           0x18
#define HID_KEY_V           0x19
#define HID_KEY_W           0x1A
#define HID_KEY_X           0x1B
#define HID_KEY_Y           0x1C
#define HID_KEY_Z           0x1D
#define HID_KEY_1           0x1E
#define HID_KEY_2           0x1F
#define HID_KEY_3           0x20
#define HID_KEY_4           0x21
#define HID_KEY_5           0x22
#define HID_KEY_6           0x23
#define HID_KEY_7           0x24
#define HID_KEY_8           0x25
#define HID_KEY_9           0x26
#define HID_KEY_0           0x27
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x20
#define HID_KEY_SPACE_REAL  0x2C  // Actual space keycode
#define HID_KEY_MINUS       0x2D
#define HID_KEY_EQUALS      0x2E
#define HID_KEY_BRACKET_L   0x2F
#define HID_KEY_BRACKET_R   0x30
#define HID_KEY_BACKSLASH   0x31
#define HID_KEY_SEMICOLON   0x33
#define HID_KEY_APOSTROPHE  0x34
#define HID_KEY_GRAVE       0x35
#define HID_KEY_COMMA       0x36
#define HID_KEY_PERIOD      0x37
#define HID_KEY_SLASH       0x38
#define HID_KEY_CAPS_LOCK   0x39
#define HID_KEY_F1          0x3A
#define HID_KEY_F2          0x3B
#define HID_KEY_F3          0x3C
#define HID_KEY_F4          0x3D
#define HID_KEY_F5          0x3E
#define HID_KEY_F6          0x3F
#define HID_KEY_F7          0x40
#define HID_KEY_F8          0x41
#define HID_KEY_F9          0x42
#define HID_KEY_F10         0x43
#define HID_KEY_F11         0x44
#define HID_KEY_F12         0x45
#define HID_KEY_PRINT_SCREEN 0x46
#define HID_KEY_SCROLL_LOCK 0x47
#define HID_KEY_PAUSE       0x48
#define HID_KEY_INSERT      0x49
#define HID_KEY_HOME        0x4A
#define HID_KEY_PAGE_UP     0x4B
#define HID_KEY_DELETE      0x4C
#define HID_KEY_END         0x4D
#define HID_KEY_PAGE_DOWN   0x4E
#define HID_KEY_ARROW_RIGHT 0x4F
#define HID_KEY_ARROW_LEFT  0x50
#define HID_KEY_ARROW_DOWN  0x51
#define HID_KEY_ARROW_UP    0x52

// ============================================================================
// MOUSE BUTTONS
// ============================================================================

#define KBMOUSE_BTN_LEFT    (1 << 0)
#define KBMOUSE_BTN_RIGHT   (1 << 1)
#define KBMOUSE_BTN_MIDDLE  (1 << 2)
#define KBMOUSE_BTN_BACK    (1 << 3)
#define KBMOUSE_BTN_FORWARD (1 << 4)

// ============================================================================
// BUTTON MAPPING TYPES
// ============================================================================

typedef enum {
    KBMOUSE_ACTION_NONE = 0,
    KBMOUSE_ACTION_KEY,         // Keyboard key press
    KBMOUSE_ACTION_MODIFIER,    // Keyboard modifier (Shift, Ctrl, etc.)
    KBMOUSE_ACTION_MOUSE_BTN,   // Mouse button click
} kbmouse_action_type_t;

// Button mapping entry
typedef struct {
    uint32_t gamepad_button;        // JP_BUTTON_* input
    kbmouse_action_type_t type;     // Action type
    uint8_t value;                  // Keycode, modifier, or mouse button
} kbmouse_button_map_t;

// ============================================================================
// ANALOG CONFIGURATION
// ============================================================================

typedef struct {
    uint8_t deadzone;           // Deadzone (0-127, default 15)
    uint8_t sensitivity;        // Sensitivity multiplier (1-10, default 5)
    uint8_t scroll_deadzone;    // Scroll deadzone (default 30)
    uint8_t scroll_speed;       // Scroll speed (1-10, default 3)
} kbmouse_analog_config_t;

// Default analog configuration
#define KBMOUSE_DEFAULT_DEADZONE        15
#define KBMOUSE_DEFAULT_SENSITIVITY     5
#define KBMOUSE_DEFAULT_SCROLL_DEADZONE 30
#define KBMOUSE_DEFAULT_SCROLL_SPEED    3

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize keyboard/mouse converter
void kbmouse_init(void);

// Convert gamepad buttons and analog values to keyboard/mouse reports
// buttons: remapped button state from profile_output_t
// profile_out: contains analog values after profile processing
// kb_report: output keyboard report
// mouse_report: output mouse report
void kbmouse_convert(uint32_t buttons,
                     const profile_output_t* profile_out,
                     kbmouse_keyboard_report_t* kb_report,
                     kbmouse_mouse_report_t* mouse_report);

// Get/set analog configuration
const kbmouse_analog_config_t* kbmouse_get_config(void);
void kbmouse_set_config(const kbmouse_analog_config_t* config);

// Get keyboard LED state (Caps Lock, Num Lock, etc.)
// Returns bitmask: bit 0 = Num Lock, bit 1 = Caps Lock, bit 2 = Scroll Lock
uint8_t kbmouse_get_led_state(void);

// Set keyboard LED state (called from USB HID output report callback)
void kbmouse_set_led_state(uint8_t leds);

#endif // KBMOUSE_H
