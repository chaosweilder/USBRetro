// cdc.c - USB CDC (Virtual Serial Port) implementation
// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Robert Dale Smith

#include "cdc.h"
#include "cdc_protocol.h"
#include "cdc_commands.h"
#include "../usbd.h"
#include "core/services/storage/flash.h"
#include "tusb.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef CONFIG_GC2ETH
#include "apps/gc2eth/ch9120.h"
#include "apps/gc2eth/app.h"
#endif

#ifdef CONFIG_GC2ETH_FEATHER
#include "apps/gc2eth_feather/w5500.h"
#include "apps/gc2eth_feather/app.h"
#endif

#if CFG_TUD_CDC > 0

// Command buffer for text commands (legacy support)
#define CMD_BUFFER_SIZE 64
static char cmd_buffer[CMD_BUFFER_SIZE];
static uint8_t cmd_pos = 0;

// Protocol mode detection
static bool binary_mode = false;  // Switch to binary after receiving 0xAA

// ============================================================================
// INITIALIZATION
// ============================================================================

void cdc_init(void)
{
    binary_mode = false;

    // Initialize binary protocol command handlers
    cdc_commands_init();
}

// Process a complete command line
static void cdc_process_command(const char* cmd)
{
    char response[128];

    // MODE? - Query current mode
    if (strcmp(cmd, "MODE?") == 0) {
        usb_output_mode_t mode = usbd_get_mode();
        snprintf(response, sizeof(response), "MODE=%d (%s)\r\n",
                 (int)mode, usbd_get_mode_name(mode));
        cdc_data_write_str(response);
    }
    // MODE=N - Set mode by number
    else if (strncmp(cmd, "MODE=", 5) == 0) {
        const char* value = cmd + 5;
        int mode_num = -1;

        // Try parsing as number first
        if (value[0] >= '0' && value[0] <= '9') {
            mode_num = atoi(value);
        }
        // Try parsing mode names
        else if (strcasecmp(value, "HID") == 0 || strcasecmp(value, "DINPUT") == 0) {
            mode_num = USB_OUTPUT_MODE_HID;
        }
        else if (strcasecmp(value, "XOG") == 0 || strcasecmp(value, "XBOX_OG") == 0 ||
                 strcasecmp(value, "XBOX") == 0) {
            mode_num = USB_OUTPUT_MODE_XBOX_ORIGINAL;
        }
        else if (strcasecmp(value, "XAC") == 0 || strcasecmp(value, "ADAPTIVE") == 0) {
            mode_num = USB_OUTPUT_MODE_XAC;
        }

        if (mode_num >= 0 && mode_num < USB_OUTPUT_MODE_COUNT) {
            usb_output_mode_t current = usbd_get_mode();
            if ((usb_output_mode_t)mode_num == current) {
                snprintf(response, sizeof(response), "OK: Already in mode %d (%s)\r\n",
                         mode_num, usbd_get_mode_name((usb_output_mode_t)mode_num));
                cdc_data_write_str(response);
            } else {
                snprintf(response, sizeof(response), "OK: Switching to mode %d (%s)...\r\n",
                         mode_num, usbd_get_mode_name((usb_output_mode_t)mode_num));
                cdc_data_write_str(response);
                cdc_data_flush();
                // This will trigger a device reset
                usbd_set_mode((usb_output_mode_t)mode_num);
            }
        } else {
            snprintf(response, sizeof(response), "ERR: Invalid mode '%s'\r\n", value);
            cdc_data_write_str(response);
        }
    }
    // MODES - List available modes
    else if (strcmp(cmd, "MODES") == 0 || strcmp(cmd, "MODES?") == 0) {
#ifdef CONFIG_NGC
        cdc_data_write_str("Available modes:\r\n");
        cdc_data_write_str("  13: CDC (config only)\r\n");
#else
        cdc_data_write_str("Available modes:\r\n");
        cdc_data_write_str("  0: DInput - default\r\n");
        cdc_data_write_str("  1: Xbox Original (XID)\r\n");
        cdc_data_write_str("  2: XInput\r\n");
        cdc_data_write_str("  3: PS3\r\n");
        cdc_data_write_str("  4: PS4\r\n");
        cdc_data_write_str("  5: Switch\r\n");
        cdc_data_write_str("  6: PS Classic\r\n");
        cdc_data_write_str("  7: Xbox One\r\n");
        cdc_data_write_str("  8: XAC Compat (not in toggle)\r\n");
#endif
    }
    // VERSION or VER? - Query firmware version
    else if (strcmp(cmd, "VERSION") == 0 || strcmp(cmd, "VER?") == 0) {
        cdc_data_write_str("Joypad USB Device\r\n");
    }
    // FLASH? - Check raw flash contents
    else if (strcmp(cmd, "FLASH?") == 0) {
        flash_t flash_data;
        if (flash_load(&flash_data)) {
            snprintf(response, sizeof(response),
                     "Flash: magic=0x%08X, profile=%d, usb_mode=%d\r\n",
                     (unsigned int)flash_data.magic,
                     flash_data.active_profile_index,
                     flash_data.usb_output_mode);
            cdc_data_write_str(response);
        } else {
            cdc_data_write_str("Flash: No valid data (magic mismatch)\r\n");
        }
    }
    // HELP
    else if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
        cdc_data_write_str("Commands:\r\n");
        cdc_data_write_str("  MODE?     - Query current output mode\r\n");
        cdc_data_write_str("  MODE=N    - Set output mode (0-5 or name)\r\n");
        cdc_data_write_str("  MODES     - List available modes\r\n");
        cdc_data_write_str("  VERSION   - Show firmware version\r\n");
        cdc_data_write_str("  HELP      - Show this help\r\n");
        cdc_data_write_str("  BOOTSEL   - Reboot into UF2 bootloader\r\n");
#ifdef CONFIG_GC2ETH
        cdc_data_write_str("  IP?       - Query CH9120 assigned IP address\r\n");
        cdc_data_write_str("  TCP?      - Query CH9120 TCP connection status\r\n");
#endif
#ifdef CONFIG_GC2ETH_FEATHER
        cdc_data_write_str("  W5500?    - Dump W5500 chip state (version, link, IP, MAC, sock0)\r\n");
        cdc_data_write_str("  TCP?      - Query sock0 status register\r\n");
        cdc_data_write_str("  FRAMES?   - Show Dolphin command activity counters\r\n");
#endif
    }
    // BOOTSEL — drop into UF2 bootloader. Works from any app since
    // reset_usb_boot is part of the bootrom.
    else if (strcmp(cmd, "BOOTSEL") == 0) {
        cdc_data_write_str("Rebooting to bootloader...\r\n");
        cdc_data_flush();
        reset_usb_boot(0, 0);
    }
#ifdef CONFIG_GC2ETH
    // IP? — read CH9120's assigned IP. Drops the chip back into config
    // mode briefly to query, so doesn't work mid-Dolphin-session.
    else if (strcmp(cmd, "IP?") == 0) {
        uint8_t ip[4];
        if (ch9120_read_ip(ip)) {
            snprintf(response, sizeof(response),
                     "IP=%u.%u.%u.%u  port=54970\r\n",
                     ip[0], ip[1], ip[2], ip[3]);
            cdc_data_write_str(response);
        } else {
            cdc_data_write_str("ERR: CH9120 read_ip failed\r\n");
        }
    }
    // TCP? — non-intrusive: just reads the TCPCS pin state.
    else if (strcmp(cmd, "TCP?") == 0) {
        snprintf(response, sizeof(response), "TCP=%s\r\n",
                 ch9120_is_connected() ? "connected" : "idle");
        cdc_data_write_str(response);
    }
    // FRAMES? — Dolphin command activity counter. Tells us whether
    // CH9120 is actually forwarding TCP bytes through to UART1.
    else if (strcmp(cmd, "FRAMES?") == 0) {
        gc2eth_diag_t d;
        gc2eth_get_diag(&d);
        snprintf(response, sizeof(response),
                 "frames=%lu last_cmd=0x%02x last_n=%d "
                 "last_rx=%02x%02x%02x%02x%02x\r\n",
                 (unsigned long)d.frames_seen, d.last_cmd, d.last_n,
                 d.last_rx[0], d.last_rx[1], d.last_rx[2],
                 d.last_rx[3], d.last_rx[4]);
        cdc_data_write_str(response);
    }
    // POKE — push a sentinel byte sequence over UART1 to the CH9120
    // chip. If the chip is forwarding UART→TCP correctly, the probe
    // listener will see it. Tests the OPPOSITE direction from FRAMES.
    else if (strcmp(cmd, "POKE") == 0) {
        const uint8_t poke[] = { 0xDE, 0xAD, 0xBE, 0xEF };
        ch9120_send_bytes(poke, sizeof(poke));
        cdc_data_write_str("poked DEADBEEF to UART1\r\n");
    }
#endif
#ifdef CONFIG_GC2ETH_FEATHER
    // W5500? — dump everything we can read back from the chip. The
    // first line we care about is VERSIONR: if it isn't 0x04 the SPI
    // wiring or clock is wrong and nothing else matters. PHYCFGR bit 0
    // is link-up.
    else if (strcmp(cmd, "W5500?") == 0) {
        w5500_diag_t d;
        w5500_get_diag(&d);
        snprintf(response, sizeof(response),
                 "VERSIONR=0x%02x PHYCFGR=0x%02x link=%s\r\n",
                 d.versionr, d.phycfgr, (d.phycfgr & 0x01) ? "up" : "down");
        cdc_data_write_str(response);
        snprintf(response, sizeof(response),
                 "IP=%u.%u.%u.%u  MAC=%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 d.sipr[0], d.sipr[1], d.sipr[2], d.sipr[3],
                 d.shar[0], d.shar[1], d.shar[2], d.shar[3], d.shar[4], d.shar[5]);
        cdc_data_write_str(response);
        snprintf(response, sizeof(response),
                 "sock0: mr=0x%02x port=%u sr=0x%02x\r\n",
                 d.sn_mr, d.sn_port, d.sn_sr);
        cdc_data_write_str(response);
    }
    // TCP? — non-intrusive: just reads Sn_SR.
    else if (strcmp(cmd, "TCP?") == 0) {
        uint8_t sr = w5500_sock0_status();
        const char* name =
            sr == 0x00 ? "CLOSED" :
            sr == 0x13 ? "INIT" :
            sr == 0x14 ? "LISTEN" :
            sr == 0x17 ? "ESTABLISHED" :
            sr == 0x1C ? "CLOSE_WAIT" : "?";
        snprintf(response, sizeof(response), "TCP=0x%02x (%s)\r\n", sr, name);
        cdc_data_write_str(response);
    }
    // FRAMES? — Dolphin command activity counter for the W5500 path.
    else if (strcmp(cmd, "FRAMES?") == 0) {
        gc2eth_diag_t d;
        gc2eth_get_diag(&d);
        snprintf(response, sizeof(response),
                 "frames=%lu last_cmd=0x%02x last_n=%d "
                 "last_rx=%02x%02x%02x%02x%02x\r\n",
                 (unsigned long)d.frames_seen, d.last_cmd, d.last_n,
                 d.last_rx[0], d.last_rx[1], d.last_rx[2],
                 d.last_rx[3], d.last_rx[4]);
        cdc_data_write_str(response);
    }
#endif
    // Unknown command
    else if (strlen(cmd) > 0) {
        snprintf(response, sizeof(response), "ERR: Unknown command '%s'\r\n", cmd);
        cdc_data_write_str(response);
    }
}

void cdc_task(void)
{
    cdc_protocol_t* proto = cdc_commands_get_protocol();

    // Handle rumble auto-stop, log drain, etc.
    cdc_commands_task();

    // Process incoming data on the data port
    while (cdc_data_available() > 0) {
        int32_t ch = cdc_data_read_byte();
        if (ch < 0) break;

        // Check for binary protocol sync byte
        if (ch == CDC_SYNC_BYTE && !binary_mode) {
            binary_mode = true;
            cmd_pos = 0;  // Clear any pending text
        }

        if (binary_mode) {
            // Binary framed protocol
            cdc_protocol_rx_byte(proto, (uint8_t)ch);
        } else {
            // Legacy text protocol
            // Handle end of line (CR or LF)
            if (ch == '\r' || ch == '\n') {
                if (cmd_pos > 0) {
                    cmd_buffer[cmd_pos] = '\0';
                    cdc_process_command(cmd_buffer);
                    cmd_pos = 0;
                }
            }
            // Handle backspace
            else if (ch == '\b' || ch == 0x7F) {
                if (cmd_pos > 0) {
                    cmd_pos--;
                }
            }
            // Accumulate characters
            else if (cmd_pos < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_pos++] = (char)ch;
            }
        }
    }
}

// ============================================================================
// DATA PORT (CDC 0)
// ============================================================================

bool cdc_data_connected(void)
{
    return tud_cdc_n_connected(CDC_PORT_DATA);
}

uint32_t cdc_data_available(void)
{
    return tud_cdc_n_available(CDC_PORT_DATA);
}

uint32_t cdc_data_read(uint8_t* buffer, uint32_t bufsize)
{
    return tud_cdc_n_read(CDC_PORT_DATA, buffer, bufsize);
}

int32_t cdc_data_read_byte(void)
{
    uint8_t ch;
    if (tud_cdc_n_read(CDC_PORT_DATA, &ch, 1) == 1) {
        return ch;
    }
    return -1;
}

uint32_t cdc_data_write(const uint8_t* buffer, uint32_t bufsize)
{
    if (!tud_cdc_n_connected(CDC_PORT_DATA)) {
        return 0;
    }
    // Check available space — drop the entire packet rather than partial write
    uint32_t avail = tud_cdc_n_write_available(CDC_PORT_DATA);
    if (avail < bufsize) {
        return 0;  // Drop packet, don't corrupt the stream
    }
    uint32_t written = tud_cdc_n_write(CDC_PORT_DATA, buffer, bufsize);
    tud_cdc_n_write_flush(CDC_PORT_DATA);
    return written;
}

uint32_t cdc_data_write_str(const char* str)
{
    return cdc_data_write((const uint8_t*)str, strlen(str));
}

void cdc_data_flush(void)
{
    tud_cdc_n_write_flush(CDC_PORT_DATA);
}

// ============================================================================
// TINYUSB CDC CALLBACKS
// ============================================================================

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
    // Data available - will be read via cdc_data_read()
}

// Invoked when CDC TX is complete
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    (void)itf;
}

// Invoked when CDC line state changed (DTR/RTS)
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)dtr;
    (void)rts;
}

// Invoked when CDC line coding changed (baud, parity, etc)
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding)
{
    (void)itf;
    (void)p_line_coding;
}

#else // CFG_TUD_CDC == 0

// Stub implementations when CDC is disabled
void cdc_init(void) {}
void cdc_task(void) {}
bool cdc_data_connected(void) { return false; }
uint32_t cdc_data_available(void) { return 0; }
uint32_t cdc_data_read(uint8_t* buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
int32_t cdc_data_read_byte(void) { return -1; }
uint32_t cdc_data_write(const uint8_t* buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
uint32_t cdc_data_write_str(const char* str) { (void)str; return 0; }
void cdc_data_flush(void) {}

#endif // CFG_TUD_CDC
