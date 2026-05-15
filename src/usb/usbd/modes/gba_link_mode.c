// gba_link_mode.c - USB Vendor-class GBA Link Cable bridge for Dolphin
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Robert Dale Smith
//
// Mode driver for USB_OUTPUT_MODE_GBA_LINK. Exposes a vendor-class USB
// device that a forked Dolphin emulator talks to via libusb. Bridges
// raw joybus byte traffic between Dolphin and the connected GBA on
// joybus pin GP4 (or whatever GC_PIN_DATA is set to in the build).
//
// Wire protocol — same raw joybus bytes as the existing TCP path:
//   Dolphin → bridge: 1 byte cmd (or 5 bytes for WRITE = cmd + 4 data)
//   bridge → Dolphin: 3 bytes (RESET/STATUS reply), 5 bytes (READ),
//                     or 1 byte (WRITE jstat)
//
// Phase A skeleton: registers the descriptor + a no-op `send_report`
// hook so the rest of usbd.c can pick this mode without crashing. The
// actual joybus dispatch lives in `gba_link_mode_task()` (Step A2).

#include "tusb.h"
#include "device/usbd.h"

// Only build the dispatch path on apps that actually link the joybus
// bridge backend (gc2usb / gc2eth / gc2eth_feather). Other apps with
// CFG_TUD_VENDOR enabled but no joybus_bridge.c would get linker errors.
#if CFG_TUD_VENDOR && defined(CONFIG_JOYBUS_BRIDGE)

#include "../usbd_mode.h"
#include "../usbd.h"
#include "../descriptors/gba_link_descriptors.h"
#include "native/host/gc/joybus_bridge.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// DESCRIPTOR ACCESSORS
// ============================================================================

static const uint8_t* gba_link_get_device_descriptor(void) {
    return (const uint8_t*)&gba_link_device_descriptor;
}

static const uint8_t* gba_link_get_config_descriptor(void) {
    return gba_link_config_descriptor;
}

// ============================================================================
// LIFECYCLE
// ============================================================================

static void gba_link_init(void) {
    // Take exclusive ownership of the joybus port from gc_host so its
    // autoboot path doesn't fight Dolphin for the bus. Same pattern
    // gc2eth_feather uses.
    if (!joybus_bridge_start()) {
        printf("[gba_link] joybus_bridge_start() failed — bus owned elsewhere\n");
    } else {
        printf("[gba_link] vendor mode active, joybus owned, awaiting Dolphin\n");
    }
}

// ============================================================================
// PROTOCOL DISPATCH
// ============================================================================
//
// Joybus command lengths — same table the TCP path uses. Mirrors
// dolphin_cmd_lengths() in apps/gc2eth_feather/app.c so behavior is
// identical, only the transport (USB vs TCP) differs.
static void gba_link_cmd_lengths(uint8_t cmd, int* tx_total, int* rx_len) {
    switch (cmd) {
        case 0xFF: case 0x00: *tx_total = 1; *rx_len = 3; break;  // RESET / STATUS
        case 0x14:            *tx_total = 1; *rx_len = 5; break;  // READ
        case 0x15:            *tx_total = 5; *rx_len = 1; break;  // WRITE
        default:              *tx_total = 1; *rx_len = 1; break;  // unknown
    }
}

// Statistics — exposed via CDC (`GBALINK?`) for sanity / latency
// debugging. Atomic 32-bit reads on Cortex-M0+ are fine without locks.
static volatile uint32_t s_frames_seen   = 0;
static volatile uint32_t s_short_tx      = 0;
static volatile uint32_t s_joybus_to     = 0;
static volatile uint8_t  s_last_cmd      = 0;
static volatile int8_t   s_last_n        = 0;
static volatile uint8_t  s_last_rx[5]    = {0};

uint32_t gba_link_mode_get_frames(void)    { return s_frames_seen; }
uint32_t gba_link_mode_get_short_tx(void)  { return s_short_tx; }
uint32_t gba_link_mode_get_joybus_to(void) { return s_joybus_to; }
uint8_t  gba_link_mode_get_last_cmd(void)  { return s_last_cmd; }
int      gba_link_mode_get_last_n(void)    { return (int)s_last_n; }
void     gba_link_mode_get_last_rx(uint8_t out[5]) {
    for (int i = 0; i < 5; i++) out[i] = s_last_rx[i];
}

// Process one command from the vendor bulk-out endpoint. Returns true
// if a command was processed; false if no bytes were available. Called
// in a tight loop from gba_link_task().
static bool process_one_frame(void) {
    // Need at least 1 byte (the command byte) to know what to do.
    if (tud_vendor_n_available(0) < 1) return false;

    uint8_t cmd_byte = 0;
    if (tud_vendor_n_read(0, &cmd_byte, 1) != 1) return false;
    s_last_cmd = cmd_byte;

    int tx_total = 1, rx_len = 1;
    gba_link_cmd_lengths(cmd_byte, &tx_total, &rx_len);

    // Read the rest of the command payload (4 more bytes for WRITE).
    // Bounded wait — Dolphin should send the whole 5-byte WRITE in one
    // bulk packet so this should drain immediately, but we tolerate
    // brief gaps.
    uint8_t tx[5] = { cmd_byte };
    if (tx_total > 1) {
        int got = 0;
        absolute_time_t deadline = make_timeout_time_us(5000);
        while (got < tx_total - 1) {
            uint32_t n = tud_vendor_n_read(0, tx + 1 + got, tx_total - 1 - got);
            if (n > 0) { got += (int)n; continue; }
            if (time_reached(deadline)) break;
            tud_task();  // keep the USB stack ticking
        }
        if (got != tx_total - 1) {
            s_short_tx++;
            return false;
        }
    }

    // Forward to the GBA via joybus. READ (0x14) carries the Kawasedo
    // session_key from the BIOS which has computation latency; give it
    // 5 ms. Other commands respond inside ~250 µs.
    uint8_t rx[5] = {0};
    const uint32_t to_us = (cmd_byte == 0x14) ? 5000 : 1000;
    int n = joybus_bridge_xfer(tx, (uint16_t)tx_total,
                               rx, (uint16_t)rx_len, to_us);
    if (n < 0) {
        // Joybus timeout or GBA disconnected — reply with zero-fill so
        // Dolphin's read doesn't block forever.
        s_joybus_to++;
        memset(rx, 0, rx_len);
        n = rx_len;
    } else {
        for (int i = n; i < rx_len; i++) rx[i] = 0;
    }

    // Send the reply back to Dolphin.
    tud_vendor_n_write(0, rx, rx_len);
    tud_vendor_n_write_flush(0);

    s_frames_seen++;
    s_last_n = (int8_t)n;
    for (int i = 0; i < 5; i++) s_last_rx[i] = (i < rx_len) ? rx[i] : 0;
    return true;
}

static void gba_link_task(void) {
    // Drain everything that's pending on the vendor bulk-out endpoint.
    while (process_one_frame()) { /* keep going while data flows */ }
}

// ============================================================================
// REPORT SENDING — N/A for vendor mode (no controller HID)
// ============================================================================

static bool gba_link_send_report(uint8_t player_index,
                                 const input_event_t* event,
                                 const profile_output_t* profile_out,
                                 uint32_t buttons) {
    (void)player_index; (void)event; (void)profile_out; (void)buttons;
    // Not a HID gamepad — bridge mode does not consume controller events.
    return true;
}

static bool gba_link_is_ready(void) {
    return tud_vendor_n_mounted(0);
}

// ============================================================================
// MODE STRUCT
// ============================================================================

const usbd_mode_t gba_link_mode = {
    .name                  = "GBA Link (Dolphin)",
    .mode                  = USB_OUTPUT_MODE_GBA_LINK,
    .get_device_descriptor = gba_link_get_device_descriptor,
    .get_config_descriptor = gba_link_get_config_descriptor,
    .get_report_descriptor = NULL,                  // vendor class, no HID
    .init                  = gba_link_init,
    .send_report           = gba_link_send_report,
    .is_ready              = gba_link_is_ready,
    .handle_output         = NULL,
    .get_rumble            = NULL,
    .get_feedback          = NULL,
    .get_report            = NULL,
    .get_class_driver      = NULL,                  // tinyusb's built-in vendor class
    .task                  = gba_link_task,
};

#endif // CFG_TUD_VENDOR && CONFIG_JOYBUS_BRIDGE
