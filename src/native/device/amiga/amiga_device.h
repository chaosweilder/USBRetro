// amiga_device.h - Amiga/CD32 console output driver for JoypadOS
//
// Supports three output modes:
//   CD32 MODE     - Full 7-button serial protocol via PIO
//   JOYSTICK MODE - Standard 2-button DE9 joystick
//   PLATFORMER    - 1-button + button 2 clones UP direction
//
// Hardware connections (after TXS0108E level shifting to 5V):
//   DE9 Pin 1 = UP         (GPIO output, active LOW)
//   DE9 Pin 2 = DOWN       (GPIO output, active LOW)
//   DE9 Pin 3 = LEFT       (GPIO output, active LOW)
//   DE9 Pin 4 = RIGHT      (GPIO output, active LOW)
//   DE9 Pin 5 = JOYMODE    (GPIO input  — Amiga pulls LOW to request CD32 serial)
//   DE9 Pin 6 = CLK/FIRE1  (GPIO input in CD32 mode / GPIO output in joystick mode)
//   DE9 Pin 7 = +5V        (power from Amiga)
//   DE9 Pin 8 = GND
//   DE9 Pin 9 = DATA/FIRE2 (PIO output in CD32 mode / GPIO output in joystick mode)
//
// Pin assignments are set via CMake compile definitions:
//   AMIGA_PIN_UP, AMIGA_PIN_DOWN, AMIGA_PIN_LEFT, AMIGA_PIN_RIGHT
//   AMIGA_PIN_JOYMODE, AMIGA_PIN_CLK, AMIGA_PIN_DATA
//
// CLK and JOYMODE must be consecutive GPIOs (CLK = N, JOYMODE = N+1)
// for the PIO 'wait pin' relative addressing to work.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "core/buttons.h"
#include "core/output_interface.h"

// ============================================================================
// PIN DEFINITIONS — set via CMakeLists target_compile_definitions
// ============================================================================

// Defaults for RP2040 Zero if not specified
#ifndef AMIGA_PIN_UP
#define AMIGA_PIN_UP        2
#endif
#ifndef AMIGA_PIN_DOWN
#define AMIGA_PIN_DOWN      3
#endif
#ifndef AMIGA_PIN_LEFT
#define AMIGA_PIN_LEFT      4
#endif
#ifndef AMIGA_PIN_RIGHT
#define AMIGA_PIN_RIGHT     5
#endif
#ifndef AMIGA_PIN_CLK
#define AMIGA_PIN_CLK       6    // DE9 pin 6 — CLK input / FIRE1 output
#endif
#ifndef AMIGA_PIN_JOYMODE
#define AMIGA_PIN_JOYMODE   7    // DE9 pin 5 — must be CLK+1 for PIO
#endif
#ifndef AMIGA_PIN_DATA
#define AMIGA_PIN_DATA      8    // DE9 pin 9 — DATA output / FIRE2
#endif

// FIRE1 shares the CLK pin (joystick mode only)
#define AMIGA_PIN_FIRE1     AMIGA_PIN_CLK

// PIO input base: CLK=0 relative, JOYMODE=1 relative
#define AMIGA_PIO_IN_BASE   AMIGA_PIN_CLK

// ============================================================================
// OUTPUT MODES
// ============================================================================

typedef enum {
    AMIGA_MODE_CD32       = 0,   // Full 7-button CD32 serial protocol
    AMIGA_MODE_JOYSTICK   = 1,   // Standard 2-button DE9 joystick
    AMIGA_MODE_PLATFORMER = 2,   // 1-button + B2 clones UP direction
} amiga_output_mode_t;

// ============================================================================
// DEVICE STATE
// ============================================================================

typedef struct {
    uint32_t            buttons;
    amiga_output_mode_t mode;
} amiga_state_t;

// ============================================================================
// API
// ============================================================================

void amiga_device_init(void);
void amiga_device_task(void);
void amiga_core1_task(void);

extern const OutputInterface amiga_output_interface;
