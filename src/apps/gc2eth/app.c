// app.c — gc2eth M3: CH9120 ↔ Dolphin GBA wire protocol ↔ joybus_bridge
//
// Boot sequence:
//   1. gc_host_init()           — joybus PIO on GP_DATA (GP2 here)
//   2. joybus_bridge_start()    — take ownership of the joybus port
//   3. ch9120_init()            — CH9120 as TCP server on 54970
//   4. main loop                — parse Dolphin frames, proxy to joybus
//
// Wire protocol (verified from Dolphin SI_DeviceGBA.cpp):
//   cmd  | tx total | rx total
//   0xFF | 1        | 3        (RESET)
//   0x00 | 1        | 3        (STATUS)
//   0x14 | 1        | 5        (READ)
//   0x15 | 5        | 1        (WRITE)
//   other| 1        | 1        (default per Dolphin source)

#include "app.h"
#include "ch9120.h"
#include "wshare_ch9120.h"  // Waveshare's official CH9120 driver, verbatim
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "native/host/gc/gc_host.h"
#include "native/host/gc/joybus_bridge.h"
#include "usb/usbd/usbd.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <stdio.h>

// DIAGNOSTIC: drop usbd_output_interface entirely. The CH9120's
// TCP→UART forwarding silently stops working when the project's
// full USB device stack runs alongside it. Without an output
// interface, main.c won't init TinyUSB or call usbd_task in the
// loop. We lose CDC config access but UART stdio still works
// via GP0/GP1 if a USB-UART adapter is attached.

const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = 0;
    return NULL;
}

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = 0;
    return NULL;
}

// Dolphin GBA joybus TCP port = 0xD6BA = "dolphin gba"
#define DOLPHIN_GBA_PORT  0xD6BA
// CH9120 UART data-mode baud. Matches Waveshare's official demo's
// Transport_BAUD_RATE constant.
#define CH9120_DATA_BAUD  115200

void app_init(void)
{
    // No clock change, no second stdio_init_all — main.c already
    // brought stdio up. Doubling that call resets UART0 mid-flight
    // and the chip's nearby UART1 line can pick up glitches during
    // the re-init.

    printf("\n[app:gc2eth] %s v%s starting\n", APP_NAME, APP_VERSION);
    printf("[app:gc2eth]   board: %s\n", BOARD);
    printf("[app:gc2eth]   GC data pin: GPIO%d\n", GC_DATA_PIN);
    printf("[app:gc2eth]   CH9120: UART TX=GPIO%d RX=GPIO%d  "
           "TCPCS=GPIO%d CFG0=GPIO%d RST=GPIO%d\n",
           CH9120_UART_TX_PIN, CH9120_UART_RX_PIN,
           CH9120_TCPCS_PIN, CH9120_CFG0_PIN, CH9120_RSTI_PIN);

    // Joybus first — gc_host_init lays down the PIO program and
    // configures the data pin (GP_DATA = GP2 here).
    gc_host_init();

    joybus_bridge_init();
    if (!joybus_bridge_start()) {
        printf("[app:gc2eth] !! joybus_bridge_start failed (gc_host not ready?)\n");
    }

    // Use Waveshare's verbatim CH9120 driver — known good.
    CH9120_init();
    printf("[app:gc2eth] M3: Dolphin GBA bridge running\n");
}

// Look up tx total + rx length for a Dolphin joybus cmd byte, per the
// switch in CSIDevice_GBA::RunBuffer. Returns false for an obviously
// malformed cmd (none defined here — Dolphin's default already covers
// everything as 1+1).
static void dolphin_cmd_lengths(uint8_t cmd, int* tx_total, int* rx_len)
{
    switch (cmd) {
        case 0xFF:  // RESET
        case 0x00:  // STATUS
            *tx_total = 1; *rx_len = 3; break;
        case 0x14:  // READ
            *tx_total = 1; *rx_len = 5; break;
        case 0x15:  // WRITE
            *tx_total = 5; *rx_len = 1; break;
        default:
            *tx_total = 1; *rx_len = 1; break;
    }
}

// Diagnostic counters — printed by the heartbeat so we can see whether
// the bridge is being driven at all without spamming a line per byte.
static volatile uint32_t g_frames_seen = 0;
static volatile uint8_t  g_last_cmd = 0;
static volatile uint8_t  g_last_rx[5] = {0};
static volatile int      g_last_n = 0;

void gc2eth_get_diag(gc2eth_diag_t* out)
{
    out->frames_seen = g_frames_seen;
    out->last_cmd    = g_last_cmd;
    out->last_n      = g_last_n;
    for (int i = 0; i < 5; i++) out->last_rx[i] = g_last_rx[i];
}

// Returns true if a Dolphin command was processed this call,
// false if no byte was pending. Caller can loop to drain.
static bool process_dolphin_frame_one(void)
{
    int cmd_byte = ch9120_recv_byte_timeout(0);
    if (cmd_byte < 0) return false;
    g_frames_seen++;
    g_last_cmd = (uint8_t)cmd_byte;

    uint8_t tx[5] = { (uint8_t)cmd_byte };
    int tx_total = 1, rx_len = 1;
    dolphin_cmd_lengths(tx[0], &tx_total, &rx_len);

    // Pull the rest of a multi-byte cmd (currently only WRITE, 5 bytes
    // total). Generous per-byte timeout — Dolphin sends them back to
    // back so this is essentially instant on a healthy link.
    if (tx_total > 1) {
        int got = ch9120_recv_bytes(tx + 1, tx_total - 1, 5000);
        if (got != tx_total - 1) {
            printf("[gc2eth] short tx for cmd 0x%02x: got %d/%d — dropping\n",
                   tx[0], got, tx_total - 1);
            return false;
        }
    }

    // Hand off to joybus. 1 ms timeout matches gba_multiboot's per-WRITE
    // budget; if the GBA isn't responding, we fall back to zeros so the
    // protocol stays in lock-step.
    uint8_t rx[5];
    int n = joybus_bridge_xfer(tx, (uint16_t)tx_total,
                               rx, (uint16_t)rx_len,
                               /*timeout_us=*/1000);
    if (n < 0) {
        for (int i = 0; i < rx_len; i++) rx[i] = 0;
        n = rx_len;
    } else {
        for (int i = n; i < rx_len; i++) rx[i] = 0;
    }
    ch9120_send_bytes(rx, rx_len);

    g_last_n = n;
    for (int i = 0; i < 5; i++) g_last_rx[i] = (i < rx_len) ? rx[i] : 0;
    return true;
}

void app_task(void)
{
    // Reboot-to-bootloader on serial 'B'.
    int c = getchar_timeout_us(0);
    if (c == 'B') reset_usb_boot(0, 0);

    // Drain pending Dolphin commands. Loop because Dolphin may
    // burst multiple bytes per UART poll — single-byte-per-call
    // would let RX FIFO grow.
    while (process_dolphin_frame_one()) { /* keep draining */ }

    // 1 Hz heartbeat + TCPCS poll so we can see when Dolphin attaches.
    static absolute_time_t next_beat;
    static bool beat_init = false;
    static uint32_t tick = 0;
    static bool last_connected = false;
    if (!beat_init) {
        next_beat = make_timeout_time_ms(1000);
        beat_init = true;
    }
    if (time_reached(next_beat)) {
        next_beat = make_timeout_time_ms(1000);
        bool now = ch9120_is_connected();
        if (now != last_connected) {
            printf("[app:gc2eth] TCP peer %s\n", now ? "CONNECTED" : "disconnected");
            last_connected = now;
        }
        printf("[app:gc2eth] tick %lu tcp=%s frames=%lu last_cmd=0x%02x "
               "last_n=%d last_rx=%02x%02x%02x%02x%02x\n",
               (unsigned long)tick++, now ? "up" : "down",
               (unsigned long)g_frames_seen, g_last_cmd,
               g_last_n,
               g_last_rx[0], g_last_rx[1], g_last_rx[2], g_last_rx[3], g_last_rx[4]);
    }
}
