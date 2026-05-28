// jvs_host.c - Native JVS Controller Host Driver
//
// Polls native JVS controllers and submits input events to the router.

#include "jvs_host.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "core/services/hotkeys/hotkeys.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/buttons.h"
#include "hardware/structs/sio.h"
#include "jvsio_host.h"
#include "jvsio_client.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <stdio.h>

// ============================================================================
// INTERNAL STATE
// ============================================================================

typedef struct {
    int8_t type;
} jvs_pad_t;

static jvs_pad_t jvs_pads[JVS_MAX_PLAYERS];
static bool initialized = false;
// Track previous state for edge detection
static uint32_t prev_buttons[JVS_MAX_PLAYERS] = {0};
static uint32_t coin_release_time[JVS_MAX_PLAYERS] = {0};
static uint8_t jvs_comm_method = 0;
static const uint JVS_COMM_SPEEDS[3] = { 115200, 1000000, 3000000 };

// ============================================================================
// JVS Client Functions
// ============================================================================

int JVSIO_Client_isDataAvailable(void) {
    return uart_is_readable(JVS_UART);
}

uint8_t JVSIO_Client_receive(void) {
    return uart_getc(JVS_UART);
}

void JVSIO_Client_send(uint8_t data) {
    uart_putc(JVS_UART, data);
}

void JVSIO_Client_willSend(void) {
    gpio_put(PIN_JVS_RE, 1);
    if (jvs_comm_method == 0) {
        busy_wait_us_32(200);
    }
    gpio_put(PIN_JVS_DE, 1);
}

void JVSIO_Client_willReceive(void) {
    uart_tx_wait_blocking(JVS_UART);
    gpio_put(PIN_JVS_DE, 0); 
    gpio_put(PIN_JVS_RE, 0);
}

bool JVSIO_Client_isSenseConnected(void) {
    return !gpio_get(PIN_JVS_SENSE_IN_HIGH);
}

uint32_t JVSIO_Client_getTick(void) {
    return to_ms_since_boot(get_absolute_time());
}

bool JVSIO_Client_isSenseReady(void) { 
    return !gpio_get(PIN_JVS_SENSE_IN_HIGH) && !gpio_get(PIN_JVS_SENSE_IN_LOW); 
}


void JVSIO_Client_dump(const char* str, uint8_t* data, uint8_t len) {
#if JVS_DEBUG
    printf("%s: ", str);
    for (uint8_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");
#endif
}

void JVSIO_Client_ioIdReceived(uint8_t address, uint8_t* id, uint8_t len) {
    printf("[JVS HOST] Node %d - I/O Identification: ", address);
    
    for (uint8_t i = 0; i < len; i++) {
        // Only print printable characters
        if (id[i] >= 0x20 && id[i] <= 0x7E) {
            printf("%c", id[i]);
        } else {
            printf("[%02X]", id[i]);
        }
    }
    printf("\n");
}
void JVSIO_Client_commandRevReceived(uint8_t address, uint8_t rev) {
    // Reads the revision of the command format supported by the I/O board
    // The code is written in BCD, with the low 4 bits being the decimal value.
    // The current revision is REV1.3, so the value would be 0x13.
    uint8_t major = rev >> 4;
    uint8_t minor = rev & 0x0F;

    printf("[JVS HOST] Node %d - Command revision: %d.%d (Raw: 0x%02X)\n", 
            address, major, minor, rev);

    // 2. Logic Validation
    // Most modern JVS I/O boards operate under revision 1.1 (0x11) or higher.
    if (rev >= 0x11) {
        printf("[JVS HOST] Compatible command protocol detected.\n");
    } else {
        // Revision 0x10 is very old (JVS 1.0)
        printf("[JVS HOST] WARNING: Outdated command protocol version.\n");
    }
}
void JVSIO_Client_jvRevReceived(uint8_t address, uint8_t rev) {
    // The code is written in BCD, with the low 4 bits being the decimal value.
    // The current revision is REV3.0, so the value would be 0x30.
    uint8_t major = rev >> 4;
    uint8_t minor = rev & 0x0F;

    printf("[JVS HOST] Node %d - JVS revision : %d.%d (Raw: 0x%02X)\n", 
            address, major, minor, rev);

    // 2. Compatibility Logic
    if (rev >= 0x30) {
        // JVS 3.0 is the modern standard usually found in 
        // high-speed systems (like Sega Lindbergh or RingEdge).
        printf("[JVS HOST] Modern hardware compatible with extended commands.\n");
    } else if (rev == 0x20) {
        printf("[JVS HOST] Standard JVS 2.0 hardware (Naomi / Triforce).\n");
    }
}
void JVSIO_Client_protocolVerReceived(uint8_t address, uint8_t rev) {
    // Reads the version of the communication system supported by the I/O board. 
    // The code is written in BCD, with the low 4 bits being the decimal value
    // The current version is VER1.0, so the value would be 0x10.
    uint8_t major = rev >> 4;
    uint8_t minor = rev & 0x0F;

    printf("[JVS HOST] Node %d - Communications Version: %d.%d (Raw: 0x%02X)\n", 
            address, major, minor, rev);

    if (rev == 0x10) {
        printf("[JVS HOST] Standard COMMVER 1.0 detected. Proceeding...\n");
    } else {
        printf("[JVS HOST] Non-standard COMMVER detected (0x%02X). Attempting to continue...\n", rev);
    }
}


void JVSIO_Client_functionCheckReceived(uint8_t address, uint8_t* data, uint8_t len) {
    // List the features supported by the I/O board. 
    // Each function (except for end code) is 4 bytes, consisting of one function code byte and 3 parameter bytes
    printf("[JVS HOST] Parsing features supported by the I/O board %d...\n", address);

    for (uint8_t i = 0; i < len; ) {
        uint8_t type = data[i++];
        if (type == 0x00) {
            printf(" - End of list\n");
            break;
        } // End of list

        uint8_t p1 = data[i++];
        uint8_t p2 = data[i++];
        uint8_t p3 = data[i++];

        switch (type) {
            // Input Functions
            case 0x01: // Switch Input (Digital)
                printf(" - Digital: %d players, %d buttons each\n", p1, p2);
                break;

            case 0x02: // Coin Input
                printf(" - Coin Slots: %d\n", p1);
                break;

            case 0x03: // Analog Input
                printf(" - Analog: %d channels, %d-bit\n", p1, p2);
                break;

            case 0x04: // Rotary Input
                printf(" - Rotary Input: %d channels\n", p1);
                break;

            case 0x05: // Keycode Input
                printf(" - Keycode Input\n");
                break;

            case 0x06: // Screen Position Input
                printf(" - Screen Position Input: %d-Xbits, %d-Ybits, %d channels\n", p1, p2, p3);
                break;

            case 0x07: // Misc. Switch Input
                printf(" - Misc. Switch Input: SW MSB %d, SW LSB %d\n", p1, p2);
                break;

            // Output Funcions
            case 0x10: // Card System
                printf(" - Card System: %d slots\n", p1);
                break;

            case 0x11: // Medal Hopper
                printf(" - Medal Hopper: %d channels\n", p1);
                break;
            case 0x12: // General-purpose Output
                printf(" - General-purpose Output: %d slots\n", p1);
                break;

            case 0x13: // Analog Output
                printf(" - Analog Output: %d channels\n", p1);
                break;

            case 0x14: // Character Output
                printf(" - Character Output: %d width, %d height, %d type\n", p1, p2, p3);
                break;

            case 0x15: // Has Backup Data
                printf(" - Has Backup Data\n");
                break;

            default:
                printf(" - Unknown Function (0x%02X): params 0x%02X, 0x%02X\n", type, p1, p2);
                break;
        }
    }
}

// ============================================================================
// BUTTON MAPPING: JVS → JoyPad
// ============================================================================

void JVSIO_Client_synced(uint8_t players, uint8_t coin_state, uint8_t* sw_state0, uint8_t* sw_state1) {   
    // Get input for JVS_MAX_PLAYERS
    for (int i = 0; i < players && i < JVS_MAX_PLAYERS; i++) {
        // Map buttons based on device type
        // Analog axes default to 128 (center) via init_input_event below.
        // TODO: read JVS analog channels and populate event.analog[].
        uint32_t buttons = 0x00000000;

        // sw_state0[i] bits:
        // 7: Start | 6: Service | 5: Up | 4: Down | 3: Left | 2: Right | 1: B1 | 0: B2
        uint8_t b0 = sw_state0[i];

        if (b0 & 0x80) buttons |= JP_BUTTON_S2;     // Start
        if (b0 & 0x40) buttons |= JP_BUTTON_S1;     // Service
        if (b0 & 0x20) buttons |= JP_BUTTON_DU;
        if (b0 & 0x10) buttons |= JP_BUTTON_DD;
        if (b0 & 0x08) buttons |= JP_BUTTON_DL;
        if (b0 & 0x04) buttons |= JP_BUTTON_DR;
        if (b0 & 0x02) buttons |= JP_BUTTON_B3;     // Punch 1
        if (b0 & 0x01) buttons |= JP_BUTTON_B4;     // Punch 2

        // sw_state1[i] bits:
        // 7: B3 | 6: B4 | 5: B5 | 4: B6 | 3: B7 | 2: B8 | 1: L3 | 0: R3
        uint8_t b1 = sw_state1[i];

        if (b1 & 0x80) buttons |= JP_BUTTON_R1;     // Punch 3
        if (b1 & 0x40) buttons |= JP_BUTTON_B1;     // Kick 1
        if (b1 & 0x20) buttons |= JP_BUTTON_B2;     // Kick 2
        if (b1 & 0x10) buttons |= JP_BUTTON_R2;     // Kick 3
        if (b1 & 0x08) buttons |= JP_BUTTON_L1;     // B7
        if (b1 & 0x04) buttons |= JP_BUTTON_L2;     // B8
        if (b1 & 0x02) buttons |= JP_BUTTON_L3;     // L3
        if (b1 & 0x01) buttons |= JP_BUTTON_R3;     // R3

        // TEST to HOME
        if (i == 0 && coin_state & 0x80) {
           buttons |= JP_BUTTON_A1;
        }

        // COIN as SELECT — pulse for 200ms (signed diff handles tick wraparound)
        uint32_t now = JVSIO_Client_getTick();
        if (coin_state & (1 << i)) {
            coin_release_time[i] = now + 200;
        }
        if ((int32_t)(coin_release_time[i] - now) > 0) {
            buttons |= JP_BUTTON_S1;
        }

        // Only submit if state changed
        if (buttons == prev_buttons[i]) {
            continue;
        }
        prev_buttons[i] = buttons;

        // Build input event
        input_event_t event;
        init_input_event(&event);

        event.dev_addr = 0xF0 + i;  // Use 0xF0+ range for native inputs
        event.instance = 0;
        event.type = INPUT_TYPE_ARCADE_STICK;
        event.buttons = buttons;
        event.analog[ANALOG_L2] = (buttons & JP_BUTTON_L2) ? 255 : 0;
        event.analog[ANALOG_R2] = (buttons & JP_BUTTON_R2) ? 255 : 0;

        // Submit to router
        router_submit_input(&event);
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void jvs_host_init(void)
{
    for(int i=0; i<JVS_MAX_PLAYERS; i++) {
        jvs_pads[i].type = JVSPAD_ARCADE_STICK;
        prev_buttons[i] = 0xFFFFFFFF;
    }
    uart_init(JVS_UART, 115200);
    uart_set_translate_crlf(JVS_UART, false);
    gpio_set_function(PIN_JVS_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_JVS_RX, GPIO_FUNC_UART);

    gpio_init(PIN_JVS_DE);
    gpio_set_dir(PIN_JVS_DE, GPIO_OUT);
    gpio_put(PIN_JVS_DE, 0);
    gpio_init(PIN_JVS_RE);
    gpio_set_dir(PIN_JVS_RE, GPIO_OUT);
    gpio_put(PIN_JVS_RE, 0);

    gpio_init(PIN_JVS_SENSE_IN_HIGH);
    gpio_init(PIN_JVS_SENSE_IN_LOW);
    gpio_set_dir(PIN_JVS_SENSE_IN_HIGH, GPIO_IN);
    gpio_set_dir(PIN_JVS_SENSE_IN_LOW, GPIO_IN);
    gpio_pull_up(PIN_JVS_SENSE_IN_HIGH);
    gpio_pull_up(PIN_JVS_SENSE_IN_LOW);

    JVSIO_Host_init();
    initialized = true;
}

void jvs_host_task(void)
{
    JVSIO_Host_run();
    JVSIO_Host_sync();
}

int8_t jvs_host_get_device_type(uint8_t port) {
    if (port >= JVS_MAX_PLAYERS) return JVSPAD_NONE;
    return jvs_pads[port].type;
}

bool jvs_host_is_connected(void)
{
    return JVSIO_Client_isSenseConnected();
}

// ============================================================================
// INPUT INTERFACE (for app declaration)
// ============================================================================

static uint8_t jvs_get_device_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < JVS_MAX_PLAYERS; i++) {
        if (jvs_pads[i].type != JVSPAD_NONE) {
            count++;
        }
    }
    return count;
}

const InputInterface jvs_input_interface = {
    .name = "JVS",
    .source = INPUT_SOURCE_NATIVE_JVS,
    .init = jvs_host_init,
    .task = jvs_host_task,
    .is_connected = jvs_host_is_connected,
    .get_device_count = jvs_get_device_count,
};
