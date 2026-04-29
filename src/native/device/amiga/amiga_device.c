// amiga_device.c - Amiga/CD32 output driver for JoypadOS
//
// Implements CD32 7-button serial protocol using GPIO interrupts,
// based on SukkoPera's OpenPSX2AmigaPadAdapter approach (GPL v3).
//
// JOYMODE HIGH: standard 2-button joystick
// JOYMODE LOW:  CD32 serial mode, 9 bits shifted on CLK edges

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
// INTERNAL STATE
// ============================================================================

static volatile amiga_state_t amiga_state = {
    .buttons = 0,
    .mode    = AMIGA_MODE_JOYSTICK,
};

// CD32 shift register — loaded on JOYMODE falling edge, shifted on CLK
// 0=pressed, 1=released, MSB must be 1 for ID sequence
// bits: 0=Blue, 1=Red, 2=Yellow, 3=Green, 4=RFront, 5=LFront, 6=Pause, 7=1(sig)
static volatile uint8_t buttons_live = 0xFF;   // updated by tap callback
static volatile uint8_t buttons_isr  = 0xFF;   // shift register during CD32 read

// ============================================================================
// HELPER: Build CD32 byte from button bitmap
// ============================================================================

static uint8_t __not_in_flash_func(build_cd32_byte)(uint32_t buttons) {
    uint8_t b = 0xFF;  // all released + signature bit 7=1
    if (buttons & JP_BUTTON_B1) b &= ~(1 << CD32_BIT_RED);
    if (buttons & JP_BUTTON_B2) b &= ~(1 << CD32_BIT_BLUE);
    if (buttons & JP_BUTTON_B3) b &= ~(1 << CD32_BIT_GREEN);
    if (buttons & JP_BUTTON_B4) b &= ~(1 << CD32_BIT_YELLOW);
    if (buttons & JP_BUTTON_R1) b &= ~(1 << CD32_BIT_RFRONT);
    if (buttons & JP_BUTTON_L1) b &= ~(1 << CD32_BIT_LFRONT);
    if (buttons & JP_BUTTON_S2) b &= ~(1 << CD32_BIT_PAUSE);
    // bit 7 stays 1 (already set in 0xFF)
    return b;
}

// ============================================================================
// GPIO HELPERS — open-collector style like real hardware
// ============================================================================

static inline void __not_in_flash_func(pin_press)(uint pin) {
    // Pull LOW = pressed
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

static inline void __not_in_flash_func(pin_release)(uint pin) {
    // High-Z = released
    gpio_set_dir(pin, GPIO_IN);
}

static inline void __not_in_flash_func(set_dpad)(uint32_t buttons) {
    if (buttons & JP_BUTTON_DU) pin_press(AMIGA_PIN_UP);    else pin_release(AMIGA_PIN_UP);
    if (buttons & JP_BUTTON_DD) pin_press(AMIGA_PIN_DOWN);  else pin_release(AMIGA_PIN_DOWN);
    if (buttons & JP_BUTTON_DL) pin_press(AMIGA_PIN_LEFT);  else pin_release(AMIGA_PIN_LEFT);
    if (buttons & JP_BUTTON_DR) pin_press(AMIGA_PIN_RIGHT); else pin_release(AMIGA_PIN_RIGHT);
}

// ============================================================================
// GPIO INTERRUPT — JOYMODE and CLK
// ============================================================================

static void __not_in_flash_func(amiga_gpio_irq)(uint gpio, uint32_t events) {
    if (gpio == AMIGA_PIN_JOYMODE) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            // JOYMODE went LOW — CD32 mode requested

            // CLK becomes input
            gpio_set_dir(AMIGA_PIN_CLK, GPIO_IN);

            // Sample buttons_live into shift register
            buttons_isr = buttons_live >> 1;  // remaining 8 bits (bit 0 already sent)

            // Immediately output first bit (Blue = bit 0 of buttons_live)
            gpio_set_dir(AMIGA_PIN_DATA, GPIO_OUT);
            if (buttons_live & 0x01) {
                gpio_put(AMIGA_PIN_DATA, 1);  // released
            } else {
                gpio_put(AMIGA_PIN_DATA, 0);  // pressed
            }

            // Enable CLK interrupt for shifting
            gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, true);

            amiga_state.mode = AMIGA_MODE_CD32;

        } else if (events & GPIO_IRQ_EDGE_RISE) {
            // JOYMODE went HIGH — back to joystick mode

            // Disable CLK interrupt
            gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, false);

            // Restore Fire1 (CLK pin) as output
            gpio_put(AMIGA_PIN_CLK, 0);
            if (amiga_state.buttons & JP_BUTTON_B1) {
                pin_press(AMIGA_PIN_CLK);
            } else {
                pin_release(AMIGA_PIN_CLK);
            }

            // Restore Fire2 (DATA pin) as output
            gpio_put(AMIGA_PIN_DATA, 0);
            if (amiga_state.buttons & JP_BUTTON_B2) {
                pin_press(AMIGA_PIN_DATA);
            } else {
                pin_release(AMIGA_PIN_DATA);
            }

            amiga_state.mode = AMIGA_MODE_JOYSTICK;
        }

    } else if (gpio == AMIGA_PIN_CLK) {
        // CLK rising edge — shift out next bit
        if (buttons_isr & 0x01) {
            gpio_put(AMIGA_PIN_DATA, 1);
        } else {
            gpio_put(AMIGA_PIN_DATA, 0);
        }
        buttons_isr >>= 1;
    }
}

// ============================================================================
// CORE 1 TASK — not used for GPIO interrupt approach
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

    uint32_t buttons = event->buttons;
    amiga_state.buttons = buttons;

    // Always update D-pad
    set_dpad(buttons);

    // Update live button state for CD32 serial
    buttons_live = build_cd32_byte(buttons);

    // In joystick mode, also update fire buttons directly
    if (amiga_state.mode == AMIGA_MODE_JOYSTICK) {
        if (buttons & JP_BUTTON_B1) pin_press(AMIGA_PIN_CLK);
        else                        pin_release(AMIGA_PIN_CLK);

        if (buttons & JP_BUTTON_B2) pin_press(AMIGA_PIN_DATA);
        else                        pin_release(AMIGA_PIN_DATA);
    }
}

// ============================================================================
// DEVICE INIT
// ============================================================================

void amiga_device_init(void) {
    // D-pad — inputs with pull-up (open collector, HIGH-Z = released)
    gpio_init(AMIGA_PIN_UP);    gpio_put(AMIGA_PIN_UP,    0); gpio_set_dir(AMIGA_PIN_UP,    GPIO_IN);
    gpio_init(AMIGA_PIN_DOWN);  gpio_put(AMIGA_PIN_DOWN,  0); gpio_set_dir(AMIGA_PIN_DOWN,  GPIO_IN);
    gpio_init(AMIGA_PIN_LEFT);  gpio_put(AMIGA_PIN_LEFT,  0); gpio_set_dir(AMIGA_PIN_LEFT,  GPIO_IN);
    gpio_init(AMIGA_PIN_RIGHT); gpio_put(AMIGA_PIN_RIGHT, 0); gpio_set_dir(AMIGA_PIN_RIGHT, GPIO_IN);

    // CLK/FIRE1 — input (released = high-Z)
    gpio_init(AMIGA_PIN_CLK);
    gpio_put(AMIGA_PIN_CLK, 0);
    gpio_set_dir(AMIGA_PIN_CLK, GPIO_IN);

    // DATA/FIRE2 — input (released = high-Z)
    gpio_init(AMIGA_PIN_DATA);
    gpio_put(AMIGA_PIN_DATA, 0);
    gpio_set_dir(AMIGA_PIN_DATA, GPIO_IN);

    // JOYMODE — input with pull-up, interrupt on both edges
    gpio_init(AMIGA_PIN_JOYMODE);
    gpio_set_dir(AMIGA_PIN_JOYMODE, GPIO_IN);
    gpio_pull_up(AMIGA_PIN_JOYMODE);

    // Register single IRQ handler for both pins
    gpio_set_irq_enabled_with_callback(AMIGA_PIN_JOYMODE,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, amiga_gpio_irq);

    // CLK interrupt enabled only during CD32 reads (starts disabled)
    gpio_set_irq_enabled(AMIGA_PIN_CLK, GPIO_IRQ_EDGE_RISE, false);

    router_set_tap(OUTPUT_TARGET_AMIGA, amiga_tap_callback);

    amiga_state.mode = AMIGA_MODE_JOYSTICK;

    printf("[amiga] Init complete\n");
}

// ============================================================================
// DEVICE TASK
// ============================================================================

void amiga_device_task(void) {
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
