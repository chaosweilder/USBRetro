// ext_taiko.c - Taiko no Tatsujin TaTaCon drum controller report parser
//
// The TaTaCon reports four surface hits in byte 5 (active-low):
//   bit 0: reserved
//   bit 1: reserved
//   bit 2: unused
//   bit 3: Right face (don)
//   bit 4: Left  face (don)
//   bit 5: Right rim  (ka)
//   bit 6: Left  rim  (ka)
//   bit 7: unused
// Byte 4 carries Plus/Minus/Home active-low in the same bit positions as
// a Classic Controller.
//
// Velocity is not per-surface as with Rock Band drums — the TaTaCon
// reports only "hit" / "not hit" per sensor. We leave pad_velocity[] as 0
// for all indices so consumers can treat this as pure digital input.

#include "wii_ext.h"
#include <string.h>

void wii_ext_parse_taiko(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    // No analogs — zero centers.
    out->analog[WII_AXIS_LX] = 512;
    out->analog[WII_AXIS_LY] = 512;
    out->analog[WII_AXIS_RX] = 512;
    out->analog[WII_AXIS_RY] = 512;
    out->analog[WII_AXIS_LT] = 0;
    out->analog[WII_AXIS_RT] = 0;

    uint16_t b45 = (uint16_t)((~r[4] & 0xFF) << 8) | (uint16_t)((~r[5]) & 0xFF);
    uint32_t btns = 0;
    if (b45 & (1u << 11)) btns |= WII_BTN_MINUS;          // byte 4 bit 3
    if (b45 & (1u << 10)) btns |= WII_BTN_PLUS;           // byte 4 bit 2
    if (b45 & (1u <<  3)) btns |= WII_BTN_TAIKO_R_FACE;   // byte 5 bit 3 (don)
    if (b45 & (1u <<  4)) btns |= WII_BTN_TAIKO_L_FACE;   // byte 5 bit 4 (don)
    if (b45 & (1u <<  5)) btns |= WII_BTN_TAIKO_R_RIM;    // byte 5 bit 5 (ka)
    if (b45 & (1u <<  6)) btns |= WII_BTN_TAIKO_L_RIM;    // byte 5 bit 6 (ka)

    out->buttons = btns;
    memset(out->pad_velocity, 0, sizeof out->pad_velocity);
    out->has_accel = false;
    out->has_gyro = false;
}
