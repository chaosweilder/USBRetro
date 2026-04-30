// amiga_device.c - Amiga/CD32 output driver for JoypadOS
//
// Supports three modes, auto-detected from connected device type:
//   JOYSTICK MODE - Standard 2-button DE9 joystick (gamepad connected)
//   CD32 MODE     - 7-button serial protocol via GPIO interrupts (gamepad)
//   MOUSE MODE    - Quadrature mouse emulation (USB mouse connected)
//
// CD32 implementation based on SukkoPera's OpenPSX2AmigaPadAdapter (GPL v3).
//
// Amiga DE9 pinout:
//   Pin 1 = UP        (joystick) / V-pulse A (mouse)
//   Pin 2 = DOWN      (joystick) / V-pulse B (mouse)
//   Pin 3 = LEFT      (joystick) / H-pulse A (mouse)
//   Pin 4 = RIGHT     (joystick) / H-pulse B (mouse)
//   Pin 5 = JOYMODE   (CD32 mode select, input from Amiga)
//   Pin 6 = FIRE1/CLK (Fire1 output / CD32 clock input)
//   Pin 7 = +5V
//   Pin 8 = GND
//   Pin 9 = FIRE2/DATA (Fire2 output / CD32 data output / right mouse button)

#include "amiga_device.h"
#include "amiga_buttons.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/services/players/manager.h"
#include "core/services/profiles/profile.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// QUADRATURE TABLE
// Amiga mouse quadrature encoding: 2-bit gray code
// Forward:  00 -> 01 -> 11 -> 10 -> 00
// Backward: 00 -> 10 -> 11 -> 01 -> 00
// ============================================================================

static const uint8_t QUAD_TABLE[4] = { 0b00, 0b01, 0b11, 0b10 };
#define QUAD_STEPS 4

// ============================================================================
// INTERNAL STATE
// ============================================================================

static volatile amiga_state_t amiga_state = {
    .buttons = 0,
    .mode    = AMIGA_MODE_JOYSTICK,
};

// CD32 shift registers
static volatile uint8_t buttons_live = 0xFF;
static volatile uint8_t buttons_isr  = 0xFF;

// Mouse accumulator — updated by tap callback, drained by device task
static volatile int16_t mouse_accum_x = 0;
static volatile int16_t mouse_accum_y = 0;
static volatile int16_t mouse_accum_wheel = 0;  // scroll wheel accumulator

// Mouse button state — persists between events
static volatile uint32_t mouse_buttons = 0;

// Track whether a mouse is the active device — disables CD32 mode switching
static volatile bool mouse_active = false;

// Quadrature state indices
static uint8_t quad_x = 0;
static uint8_t quad_y = 0;

// ============================================================================
// HELPER: Build CD32 byte from button bitmap
// ============================================================================

static uint8_t __not_in_flash_func(build_cd32_byte)(uint32_t buttons) {
    uint8_t b = 0xFF;
    if (buttons & JP_BUTTON_B1) b &= ~(1 << CD32_BIT_RED);
    if (buttons & JP_BUTTON_B2) b &= ~(1 << CD32_BIT_BLUE);
    if (buttons & JP_BUTTON_B3) b &= ~(1 << CD32_BIT_GREEN);
    if (buttons & JP_BUTTON_B4) b &= ~(1 << CD32_BIT_YELLOW);
    if (buttons & JP_BUTTON_R1) b &= ~(1 << CD32_BIT_RFRONT);
    if (buttons & JP_BUTTON_L1) b &= ~(1 << CD32_BIT_LFRONT);
    if (buttons & JP_BUTTON_S2) b &= ~(1 << CD32_BIT_PAUSE);
    return b;
}

// ============================================================================
// GPIO HELPERS — open-collector style
// ============================================================================

static inline void __not_in_flash_func(pin_press)(uint pin) {
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

static inline void __not_in_flash_func(pin_release)(uint pin) {
    gpio_set_dir(pin, GPIO_IN);
}

static inline void __not_in_flash_func(set_dpad)(uint32_t buttons) {
    if (buttons & JP_BUTTON_DU) pin_press(AMIGA_PIN_UP);    else pin_release(AMIGA_PIN_UP);
    if (buttons & JP_BUTTON_DD) pin_press(AMIGA_PIN_DOWN);  else pin_release(AMIGA_PIN_DOWN);
    if (buttons & JP_BUTTON_DL) pin_press(AMIGA_PIN_LEFT);  else pin_release(AMIGA_PIN_LEFT);
    if (buttons & JP_BUTTON_DR) pin_press(AMIGA_PIN_RIGHT); else pin_release(AMIGA_PIN_RIGHT);
}

// ============================================================================
// QUADRATURE HELPERS
// Amiga mouse quadrature pin pairs (from hardware documentation):
//   Horizontal: PIN_RIGHT (DE9 pin 4) and PIN_DOWN (DE9 pin 2)
//   Vertical:   PIN_LEFT  (DE9 pin 3) and PIN_UP   (DE9 pin 1)
// ============================================================================

static inline void set_quad_x(uint8_t state) {
    if (state & 0x01) pin_press(AMIGA_PIN_RIGHT); else pin_release(AMIGA_PIN_RIGHT);
    if (state & 0x02) pin_press(AMIGA_PIN_DOWN);  else pin_release(AMIGA_PIN_DOWN);
}

static inline void set_quad_y(uint8_t state) {
    if (state & 0x01) pin_press(AMIGA_PIN_LEFT); else pin_release(AMIGA_PIN_LEFT);
    if (state & 0x02) pin_press(AMIGA_PIN_UP);   else pin_release(AMIGA_PIN_UP);
}

// ============================================================================
// JOYMODE + CLK GPIO INTERRUPT
// ============================================================================

static void __not_in_flash_func(amiga_gpio_irq)(uint gpio, uint32_t events) {
    if (gpio == AMIGA_PIN_JOYMODE) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (amiga_state.mode == AMIGA_MODE_JOYSTICK && !mouse_active) {
                amiga_state.mode = AMIGA_MODE_CD32;
                gpio_set_dir(AMIGA_PIN_CLK, GPIO_IN);
                buttons_isr = buttons_live >> 1;
                gpio_set_dir(AMIGA_PIN_DATA, GPIO_OUT);
                gpio_put(AMIGA_PIN_DATA, (buttons_live & 0x01) ? 1 : 0);
                gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, true);
            }
        } else if (events & GPIO_IRQ_EDGE_RISE) {
            if (amiga_state.mode == AMIGA_MODE_CD32) {
                gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, false);
                if (amiga_state.buttons & JP_BUTTON_B1) pin_press(AMIGA_PIN_CLK);
                else pin_release(AMIGA_PIN_CLK);
                if (amiga_state.buttons & JP_BUTTON_B2) pin_press(AMIGA_PIN_DATA);
                else pin_release(AMIGA_PIN_DATA);
                amiga_state.mode = AMIGA_MODE_JOYSTICK;
            }
        }
    } else if (gpio == AMIGA_PIN_CLK) {
        if (buttons_isr & 0x01) gpio_put(AMIGA_PIN_DATA, 1);
        else                    gpio_put(AMIGA_PIN_DATA, 0);
        buttons_isr >>= 1;
    }
}

// ============================================================================
// CORE 1 TASK
// ============================================================================

void __not_in_flash_func(amiga_core1_task)(void) {
    while (true) {
        tight_loop_contents();
    }
}

// ============================================================================
// INPUT EVENT TAP
// ============================================================================

static void __not_in_flash_func(amiga_tap_callback)(output_target_t output,
                                                      uint8_t player_index,
                                                      const input_event_t* event)
{
    (void)output;
    (void)player_index;

    if (event->type == INPUT_TYPE_MOUSE) {
        // Mouse mode — accumulate deltas, store button state
        mouse_active = true;
        mouse_accum_x += event->delta_x;
        mouse_accum_y += event->delta_y;
        mouse_accum_wheel += event->delta_wheel;
        mouse_buttons = event->buttons;

    } else {
        // Gamepad mode
        mouse_active = false;
        uint32_t buttons = event->buttons;
        amiga_state.buttons = buttons;

        set_dpad(buttons);
        buttons_live = build_cd32_byte(buttons);

        if (amiga_state.mode == AMIGA_MODE_JOYSTICK) {
            if (buttons & JP_BUTTON_B1) pin_press(AMIGA_PIN_CLK);
            else                        pin_release(AMIGA_PIN_CLK);
            if (buttons & JP_BUTTON_B2) pin_press(AMIGA_PIN_DATA);
            else                        pin_release(AMIGA_PIN_DATA);
        }
    }
}

// ============================================================================
// DEVICE TASK — drain mouse accumulator, generate quadrature pulses
// ============================================================================

void amiga_device_task(void) {
    // Only handle mouse state when in joystick mode (not during CD32 transactions)
    if (amiga_state.mode == AMIGA_MODE_JOYSTICK && mouse_active) {

        // Apply mouse buttons continuously so held state persists
        if (mouse_buttons & JP_BUTTON_B1) pin_press(AMIGA_PIN_CLK);
        else                              pin_release(AMIGA_PIN_CLK);

        if (mouse_buttons & JP_BUTTON_B2) pin_press(AMIGA_PIN_DATA);
        else                              pin_release(AMIGA_PIN_DATA);

        // WheelBusMouse scroll protocol:
        // Pull MMB (JOYMODE pin 5) LOW, generate Y quadrature steps for scroll
        // amount, then release MMB HIGH. Amiga driver reads Y delta while MMB
        // is LOW as scroll wheel data (not cursor movement).
        if (mouse_accum_wheel != 0) {
            int16_t steps = mouse_accum_wheel;
            mouse_accum_wheel = 0;

            // Disable JOYMODE IRQ during scroll — we're driving the pin ourselves
            gpio_set_irq_enabled(AMIGA_PIN_JOYMODE, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false);

            // Pull MMB LOW to signal scroll transaction
            pin_press(AMIGA_PIN_JOYMODE);
            busy_wait_us(50);

            // Generate quadrature steps for scroll amount
            for (int i = 0; i < (steps < 0 ? -steps : steps); i++) {
                if (steps > 0) {
                    quad_y = (quad_y + QUAD_STEPS - 1) % QUAD_STEPS;
                } else {
                    quad_y = (quad_y + 1) % QUAD_STEPS;
                }
                set_quad_y(QUAD_TABLE[quad_y]);
                busy_wait_us(10);
            }

            busy_wait_us(50);
            pin_release(AMIGA_PIN_JOYMODE);
            busy_wait_us(50);

            // Re-enable JOYMODE IRQ
            gpio_set_irq_enabled(AMIGA_PIN_JOYMODE, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
        }
    }

    // Process regular mouse movement — only when mouse is active
    if (!mouse_active || (mouse_accum_x == 0 && mouse_accum_y == 0)) return;

    // Horizontal
    if (mouse_accum_x > 0) {
        quad_x = (quad_x + QUAD_STEPS - 1) % QUAD_STEPS;
        mouse_accum_x--;
    } else if (mouse_accum_x < 0) {
        quad_x = (quad_x + 1) % QUAD_STEPS;
        mouse_accum_x++;
    }

    // Vertical
    if (mouse_accum_y > 0) {
        quad_y = (quad_y + QUAD_STEPS - 1) % QUAD_STEPS;
        mouse_accum_y--;
    } else if (mouse_accum_y < 0) {
        quad_y = (quad_y + 1) % QUAD_STEPS;
        mouse_accum_y++;
    }

    set_quad_x(QUAD_TABLE[quad_x]);
    set_quad_y(QUAD_TABLE[quad_y]);
}

// ============================================================================
// DEVICE INIT
// ============================================================================

void amiga_device_init(void) {
    // All DE9 signal pins start as inputs (open-collector, HIGH-Z = released)
    gpio_init(AMIGA_PIN_UP);    gpio_put(AMIGA_PIN_UP,    0); gpio_set_dir(AMIGA_PIN_UP,    GPIO_IN);
    gpio_init(AMIGA_PIN_DOWN);  gpio_put(AMIGA_PIN_DOWN,  0); gpio_set_dir(AMIGA_PIN_DOWN,  GPIO_IN);
    gpio_init(AMIGA_PIN_LEFT);  gpio_put(AMIGA_PIN_LEFT,  0); gpio_set_dir(AMIGA_PIN_LEFT,  GPIO_IN);
    gpio_init(AMIGA_PIN_RIGHT); gpio_put(AMIGA_PIN_RIGHT, 0); gpio_set_dir(AMIGA_PIN_RIGHT, GPIO_IN);
    gpio_init(AMIGA_PIN_CLK);   gpio_put(AMIGA_PIN_CLK,   0); gpio_set_dir(AMIGA_PIN_CLK,   GPIO_IN);
    gpio_init(AMIGA_PIN_DATA);  gpio_put(AMIGA_PIN_DATA,  0); gpio_set_dir(AMIGA_PIN_DATA,  GPIO_IN);

    // JOYMODE — input with pull-up, interrupt on both edges
    gpio_init(AMIGA_PIN_JOYMODE);
    gpio_set_dir(AMIGA_PIN_JOYMODE, GPIO_IN);
    gpio_pull_up(AMIGA_PIN_JOYMODE);
    gpio_set_irq_enabled_with_callback(AMIGA_PIN_JOYMODE,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, amiga_gpio_irq);

    // CLK interrupt starts disabled — enabled only during CD32 reads
    gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, false);

    router_set_tap(OUTPUT_TARGET_AMIGA, amiga_tap_callback);

    amiga_state.mode = AMIGA_MODE_JOYSTICK;

    printf("[amiga] Init complete\n");
}

// ============================================================================
// OUTPUT INTERFACE
// ============================================================================

const OutputInterface amiga_output_interface = {
    .name           = "Amiga/CD32",
    .init           = amiga_device_init,
    .core1_task     = amiga_core1_task,
    .task           = amiga_device_task,
    .get_rumble     = NULL,
    .get_player_led = NULL,
};
