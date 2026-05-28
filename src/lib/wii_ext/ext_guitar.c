// ext_guitar.c - Guitar Hero 3 / World Tour Guitar report parser
//
// 6-byte report layout (from wiibrew):
//     bit    7    6    5    4    3    2    1    0
//   [0]     -    -    SX5  SX4  SX3  SX2  SX1  SX0       (joystick X, 6-bit)
//   [1]     -    -    SY5  SY4  SY3  SY2  SY1  SY0       (joystick Y, 6-bit)
//   [2]     -    -    -    WB4  WB3  WB2  WB1  WB0       (whammy bar, 5-bit)
//   [3]     -    -    -    TB4  TB3  TB2  TB1  TB0       (touch bar, 5-bit; GHWT only)
//   [4]     1    1    bSD  1    bMn  bPl  1    1         (strum-down, minus, plus; all active-low)
//   [5]     bO   bR   bB   bG   bY   bSU  1    1         (frets + strum-up; active-low)
//
// User spec: touch-bar ("fret slider" on GHWT) is mapped to the same five
// fret buttons — whichever quantized zone is currently touched fires that
// fret's button (on top of any discrete fret-button state, OR'd together).
// Whammy bar is mapped to the RT analog axis.

#include "wii_ext.h"

// Touch-bar quantization thresholds from the GHWT spec — values between
// these are "sliding" and also fire both neighbouring frets briefly.
// We pick the nearest fret for the emulated button press.
static uint32_t touchbar_to_fret_mask(uint8_t tb) {
    if (tb == 0) return 0;                        // not touching
    if (tb <= 0x07) return WII_BTN_GH_GREEN;      // G
    if (tb <= 0x0C) return WII_BTN_GH_GREEN | WII_BTN_GH_RED;    // GR
    if (tb <= 0x12) return WII_BTN_GH_RED;        // R
    if (tb <= 0x15) return WII_BTN_GH_RED | WII_BTN_GH_YELLOW;   // RY
    if (tb <= 0x17) return WII_BTN_GH_YELLOW;     // Y
    if (tb <= 0x19) return WII_BTN_GH_YELLOW | WII_BTN_GH_BLUE;  // YB
    if (tb <= 0x1B) return WII_BTN_GH_BLUE;       // B
    if (tb <= 0x1D) return WII_BTN_GH_BLUE | WII_BTN_GH_ORANGE;  // BO
    return WII_BTN_GH_ORANGE;                     // O
}

void wii_ext_parse_guitar(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    // Joystick (6-bit) -> 10-bit common axis space.
    uint16_t sx = (uint16_t)(r[0] & 0x3F);
    uint16_t sy = (uint16_t)(r[1] & 0x3F);
    out->analog[WII_AXIS_LX] = (uint16_t)(sx << 4);
    out->analog[WII_AXIS_LY] = (uint16_t)(sy << 4);
    out->analog[WII_AXIS_RX] = 512;
    out->analog[WII_AXIS_RY] = 512;

    // Whammy bar (5-bit, rests near 0x10, pressed down toward 0x1F) -> RT.
    uint16_t whammy = (uint16_t)(r[2] & 0x1F);
    // Rest at ~0x10, so scale the full excursion 0..0x1F to 0..1023.
    out->analog[WII_AXIS_RT] = (uint16_t)(whammy << 5);
    out->analog[WII_AXIS_LT] = 0;

    // Buttons — inverted in bytes 4 and 5.
    uint16_t b45 = (uint16_t)((~r[4] & 0xFF) << 8) | (uint16_t)((~r[5]) & 0xFF);
    uint32_t btns = 0;
    if (b45 & (1u << 13)) btns |= WII_BTN_GH_STRUM_DOWN;  // byte 4 bit 5
    if (b45 & (1u << 11)) btns |= WII_BTN_MINUS;          // byte 4 bit 3
    if (b45 & (1u << 10)) btns |= WII_BTN_PLUS;           // byte 4 bit 2
    if (b45 & (1u << 7))  btns |= WII_BTN_GH_ORANGE;      // byte 5 bit 7
    if (b45 & (1u << 6))  btns |= WII_BTN_GH_RED;         // byte 5 bit 6
    if (b45 & (1u << 5))  btns |= WII_BTN_GH_BLUE;        // byte 5 bit 5
    if (b45 & (1u << 4))  btns |= WII_BTN_GH_GREEN;       // byte 5 bit 4
    if (b45 & (1u << 3))  btns |= WII_BTN_GH_YELLOW;      // byte 5 bit 3
    if (b45 & (1u << 2))  btns |= WII_BTN_GH_STRUM_UP;    // byte 5 bit 2

    // Touch-bar (GHWT): OR the inferred fret mask over the discrete frets.
    btns |= touchbar_to_fret_mask((uint8_t)(r[3] & 0x1F));

    out->buttons = btns;
    out->has_accel = false;
    out->has_gyro = false;
}
