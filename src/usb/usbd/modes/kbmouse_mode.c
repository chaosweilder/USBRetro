// kbmouse_mode.c - Keyboard/Mouse USB device mode
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "tusb.h"
#include "../usbd_mode.h"
#include "../usbd.h"
#include "../kbmouse/kbmouse.h"
#include "descriptors/kbmouse_descriptors.h"
#include "core/buttons.h"
#include <string.h>

// ============================================================================
// STATE
// ============================================================================

static kbmouse_keyboard_report_t kbmouse_kb_report;
static kbmouse_mouse_report_t kbmouse_mouse_report;
static bool send_keyboard_next = true;

// ============================================================================
// MODE INTERFACE IMPLEMENTATION
// ============================================================================

static void kbmouse_mode_init(void)
{
    kbmouse_init();
    memset(&kbmouse_kb_report, 0, sizeof(kbmouse_kb_report));
    memset(&kbmouse_mouse_report, 0, sizeof(kbmouse_mouse_report));
    send_keyboard_next = true;
}

static bool kbmouse_mode_is_ready(void)
{
    return tud_hid_ready();
}

static bool kbmouse_mode_send_report(uint8_t player_index,
                                      const input_event_t* event,
                                      const profile_output_t* profile_out,
                                      uint32_t buttons)
{
    (void)player_index;
    (void)event;

    // Convert gamepad to keyboard/mouse reports
    kbmouse_convert(buttons, profile_out, &kbmouse_kb_report, &kbmouse_mouse_report);

    // Alternate between keyboard and mouse reports
    if (send_keyboard_next) {
        send_keyboard_next = false;
        return tud_hid_keyboard_report(KBMOUSE_REPORT_ID_KEYBOARD,
                                       kbmouse_kb_report.modifier,
                                       kbmouse_kb_report.keycode);
    } else {
        send_keyboard_next = true;
        return tud_hid_mouse_report(KBMOUSE_REPORT_ID_MOUSE,
                                    kbmouse_mouse_report.buttons,
                                    kbmouse_mouse_report.x,
                                    kbmouse_mouse_report.y,
                                    kbmouse_mouse_report.wheel,
                                    kbmouse_mouse_report.pan);
    }
}

// Special handling for when no new input - still need to send mouse for continuous movement
bool kbmouse_mode_send_idle_mouse(void)
{
    if (!tud_hid_ready()) return false;

    if (!send_keyboard_next) {
        send_keyboard_next = true;
        return tud_hid_mouse_report(KBMOUSE_REPORT_ID_MOUSE,
                                    kbmouse_mouse_report.buttons,
                                    kbmouse_mouse_report.x,
                                    kbmouse_mouse_report.y,
                                    kbmouse_mouse_report.wheel,
                                    kbmouse_mouse_report.pan);
    }
    return false;
}

static void kbmouse_mode_handle_output(uint8_t report_id, const uint8_t* data, uint16_t len)
{
    // Keyboard LED output report (Report ID 1, 1 byte)
    // bit 0 = NumLock, bit 1 = CapsLock, bit 2 = ScrollLock
    if (report_id == KBMOUSE_REPORT_ID_KEYBOARD && len >= 1) {
        kbmouse_set_led_state(data[0]);
    }
}

static const uint8_t* kbmouse_mode_get_device_descriptor(void)
{
    return (const uint8_t*)&kbmouse_device_descriptor;
}

static const uint8_t* kbmouse_mode_get_config_descriptor(void)
{
    return kbmouse_config_descriptor;
}

static const uint8_t* kbmouse_mode_get_report_descriptor(void)
{
    return kbmouse_report_descriptor;
}

// ============================================================================
// MODE EXPORT
// ============================================================================

const usbd_mode_t kbmouse_mode = {
    .name = "KB/Mouse",
    .mode = USB_OUTPUT_MODE_KEYBOARD_MOUSE,

    .get_device_descriptor = kbmouse_mode_get_device_descriptor,
    .get_config_descriptor = kbmouse_mode_get_config_descriptor,
    .get_report_descriptor = kbmouse_mode_get_report_descriptor,

    .init = kbmouse_mode_init,
    .send_report = kbmouse_mode_send_report,
    .is_ready = kbmouse_mode_is_ready,

    .handle_output = kbmouse_mode_handle_output,
    .get_rumble = NULL,
    .get_feedback = NULL,
    .get_report = NULL,

    .get_class_driver = NULL,  // Uses built-in HID class driver
    .task = NULL,
};
