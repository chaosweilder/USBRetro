// wii_ext_types.h - Normalized Wii extension controller state
//
// Accessory-agnostic types. Each per-accessory parser lowers its native
// report format to this shared state; host adapters consume only these.

#ifndef WII_EXT_TYPES_H
#define WII_EXT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WII_EXT_TYPE_NONE = 0,
    WII_EXT_TYPE_NUNCHUCK,
    WII_EXT_TYPE_CLASSIC,
    WII_EXT_TYPE_CLASSIC_PRO,
    WII_EXT_TYPE_GUITAR,         // GH3 + GHWT (same dispatch, touch-bar parsed when present)
    WII_EXT_TYPE_DRUMS,          // Rock Band / Guitar Hero drums
    WII_EXT_TYPE_TURNTABLE,      // DJ Hero turntable
    WII_EXT_TYPE_TAIKO,          // Taiko no Tatsujin TaTaCon drum
    WII_EXT_TYPE_UDRAW,          // THQ uDraw tablet
    WII_EXT_TYPE_MOTIONPLUS,     // MotionPlus standalone or passthrough
} wii_ext_type_t;

// Button bitmask. Superset covering all accessories; unused bits per
// accessory are simply never set.
typedef enum {
    WII_BTN_A       = 1u << 0,   // Classic: a
    WII_BTN_B       = 1u << 1,   // Classic: b
    WII_BTN_X       = 1u << 2,   // Classic: x
    WII_BTN_Y       = 1u << 3,   // Classic: y
    WII_BTN_L       = 1u << 4,   // Classic: L shoulder digital
    WII_BTN_R       = 1u << 5,   // Classic: R shoulder digital
    WII_BTN_ZL      = 1u << 6,   // Classic: ZL
    WII_BTN_ZR      = 1u << 7,   // Classic: ZR
    WII_BTN_MINUS   = 1u << 8,
    WII_BTN_PLUS    = 1u << 9,
    WII_BTN_HOME    = 1u << 10,
    WII_BTN_DU      = 1u << 11,
    WII_BTN_DD      = 1u << 12,
    WII_BTN_DL      = 1u << 13,
    WII_BTN_DR      = 1u << 14,
    WII_BTN_C       = 1u << 15,  // Nunchuck
    WII_BTN_Z       = 1u << 16,  // Nunchuck
    // Guitar (GH3 / GHWT): fret buttons + strum + whammy surface via analog.
    WII_BTN_GH_GREEN  = 1u << 17,
    WII_BTN_GH_RED    = 1u << 18,
    WII_BTN_GH_YELLOW = 1u << 19,
    WII_BTN_GH_BLUE   = 1u << 20,
    WII_BTN_GH_ORANGE = 1u << 21,
    WII_BTN_GH_STRUM_UP   = 1u << 22,
    WII_BTN_GH_STRUM_DOWN = 1u << 23,
    // Drums: pad hits (velocity also in state->pad_velocity[]).
    WII_BTN_DRUM_RED    = 1u << 17,
    WII_BTN_DRUM_YELLOW = 1u << 18,
    WII_BTN_DRUM_BLUE   = 1u << 19,
    WII_BTN_DRUM_GREEN  = 1u << 20,
    WII_BTN_DRUM_ORANGE = 1u << 21,
    WII_BTN_DRUM_BASS   = 1u << 22,
    // DJ Hero turntable face buttons.
    WII_BTN_DJ_LEFT_GREEN  = 1u << 17,
    WII_BTN_DJ_LEFT_RED    = 1u << 18,
    WII_BTN_DJ_LEFT_BLUE   = 1u << 19,
    WII_BTN_DJ_RIGHT_GREEN = 1u << 20,
    WII_BTN_DJ_RIGHT_RED   = 1u << 21,
    WII_BTN_DJ_RIGHT_BLUE  = 1u << 22,
    WII_BTN_DJ_EUPHORIA    = 1u << 23,
    // Taiko drum surfaces.
    WII_BTN_TAIKO_L_FACE = 1u << 17,
    WII_BTN_TAIKO_R_FACE = 1u << 18,
    WII_BTN_TAIKO_L_RIM  = 1u << 19,
    WII_BTN_TAIKO_R_RIM  = 1u << 20,
} wii_button_t;

// Analog axes. Values are normalized to 0..1023 (10-bit) with neutral at 512.
// Triggers (LT/RT) are 0 at rest, 1023 fully pressed. Nunchuck stick uses
// LX/LY only. Classic populates all six; Classic Pro leaves LT/RT as 0.
typedef enum {
    WII_AXIS_LX = 0,
    WII_AXIS_LY = 1,
    WII_AXIS_RX = 2,
    WII_AXIS_RY = 3,
    WII_AXIS_LT = 4,
    WII_AXIS_RT = 5,
    WII_AXIS_COUNT = 6,
} wii_axis_t;

typedef struct {
    wii_ext_type_t type;
    bool     connected;
    uint32_t buttons;
    uint16_t analog[WII_AXIS_COUNT];
    // Nunchuck accelerometer and MotionPlus gyro (raw, centered at 0).
    int16_t  accel[3];
    int16_t  gyro[3];
    bool     has_accel;
    bool     has_gyro;
    // Drums: 4-bit velocity per pad, indexed by WII_DRUM_PAD_*. Only the
    // pad reported in the current poll has a non-zero value; zero means
    // "not hit this poll". Most rhythm games only need the button press
    // (in `buttons`) — velocity is surfaced for games that want it.
    uint8_t  pad_velocity[6];
    // uDraw tablet position (absolute 12-bit / 10-bit) and pen pressure.
    uint16_t tablet_x;
    uint16_t tablet_y;
    uint8_t  tablet_pressure;
    bool     tablet_active;
} wii_ext_state_t;

// Drum pad indices for `pad_velocity[]`.
enum {
    WII_DRUM_PAD_RED    = 0,
    WII_DRUM_PAD_YELLOW = 1,
    WII_DRUM_PAD_BLUE   = 2,
    WII_DRUM_PAD_GREEN  = 3,
    WII_DRUM_PAD_ORANGE = 4,
    WII_DRUM_PAD_BASS   = 5,
};

// Transport vtable. All members must be non-NULL. The protocol library is
// pure C and has no knowledge of the underlying I2C implementation.
typedef struct {
    // Write `len` bytes to `addr`. Returns 0 on ACK, non-zero on NACK.
    int  (*write)(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len);
    // Read `len` bytes from `addr`. Returns 0 on success, non-zero otherwise.
    int  (*read)(void *ctx, uint8_t addr, uint8_t *data, uint16_t len);
    // Block for at least `us` microseconds. Used between write-register and
    // read-data to satisfy clone-controller inter-transaction timing.
    void (*delay_us)(uint32_t us);
    void *ctx;
} wii_ext_transport_t;

#endif // WII_EXT_TYPES_H
