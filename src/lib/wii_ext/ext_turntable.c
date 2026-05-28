// ext_turntable.c - DJ Hero turntable report parser
//
// CAVEAT — UNTESTED against real hardware. The DJ Hero turntable has
// some of the densest bit-packing in the Wii extension family: two 6-bit
// signed turntable velocities plus a 4-bit crossfader plus a 5-bit
// effects dial are scattered across bytes 0-3 in non-contiguous bit
// fields. The extraction below follows wiibrew's documented layout, but
// the field widths and sign-extension paths (especially the split-across-
// three-bytes right-turntable reconstruction and the direction bits in
// r[2].6 / r[3].7) have not been validated on a physical unit. Expect to
// revisit:
//   - Turntable sign polarity (left vs right rotation feeling inverted)
//   - Crossfader direction (left/right of center may be swapped)
//   - Effects-dial wrap (5-bit rollover at full CW rotation)
// Crossfader AXIS_LT and Effects AXIS_RT may need ±1-bit shift tweaks.
//
// 6-byte report layout (from wiibrew):
//   [0]   RTT4 RTT3 LTT5 LTT4 LTT3 LTT2 LTT1 LTT0   (right turntable bit4,3 + left turntable 6-bit)
//   [1]   RTT4 RTT3 RTT2 RTT1 RTT0 CSR2 CSR1 CSR0   (right turntable mid bits + crossfader high 3 bits)
//   [2]   RTT5 LTTS CSB3 CSB2 EFD4 EFD3 EFD2 EFD1   (turntable direction bits + crossfader bit3 + effects high)
//   [3]   EFD0 CSB0 RTS  -    -    -    -    -      (effects dial low + right turntable direction + crossfader low)
//   [4]   buttons 1 (active-low)
//   [5]   buttons 2 (active-low)
//
// Turntable values are 6-bit signed (2s complement) representing rotational
// *velocity* — we accumulate into stick-shaped absolute axes. Crossfader is
// 4-bit (0..15), effects dial is 5-bit (0..31), scaled to our 10-bit axis.
// User spec: map rotation to analog axes. We place left turntable on LX,
// right turntable on RX (centered at 512 with accumulator clamp).

#include "wii_ext.h"

static int16_t turntable_accum_left  = 512;
static int16_t turntable_accum_right = 512;

static int8_t signed6(uint8_t v) {
    // 6-bit two's complement → int8
    return (v & 0x20) ? (int8_t)(v | 0xC0) : (int8_t)(v & 0x1F);
}

void wii_ext_parse_turntable(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    // Left turntable: r[0] bits 5..0 (6-bit signed velocity).
    int8_t ltt = signed6((uint8_t)(r[0] & 0x3F));
    // Right turntable: stitched from r[0] bits 7..6, r[1] bits 7..5, r[2] bit 7.
    uint8_t rtt_raw = (uint8_t)(((r[0] >> 3) & 0x18) |
                                ((r[1] >> 5) & 0x07));
    int8_t rtt = signed6(rtt_raw);
    // Direction bits (LTTS = r[2] bit 6, RTS = r[3] bit 7); some parsers
    // apply them as a sign flip instead of using the 6-bit signed form.
    // Most real turntables already convey sign via the 6-bit field, so we
    // stick with the 6-bit-signed interpretation and ignore the explicit
    // direction bits (they would be redundant in that case).

    // Accumulate rotation into a virtual absolute stick position.
    // Clamp to [0, 1023]; rotation returns slowly to center when idle.
    turntable_accum_left  += (int16_t)ltt * 4;
    turntable_accum_right += (int16_t)rtt * 4;
    // Spring toward center when no rotation for this sample.
    if (ltt == 0) turntable_accum_left  += (512 - turntable_accum_left) / 16;
    if (rtt == 0) turntable_accum_right += (512 - turntable_accum_right) / 16;
    if (turntable_accum_left  < 0)    turntable_accum_left  = 0;
    if (turntable_accum_left  > 1023) turntable_accum_left  = 1023;
    if (turntable_accum_right < 0)    turntable_accum_right = 0;
    if (turntable_accum_right > 1023) turntable_accum_right = 1023;

    out->analog[WII_AXIS_LX] = (uint16_t)turntable_accum_left;
    out->analog[WII_AXIS_LY] = 512;
    out->analog[WII_AXIS_RX] = (uint16_t)turntable_accum_right;
    out->analog[WII_AXIS_RY] = 512;

    // Crossfader: r[2] bits 3..2 (high) + r[1] bits 2..0 (low) -> 4-bit.
    uint8_t xf = (uint8_t)(((r[2] >> 2) & 0x0C) | (r[1] & 0x07));
    // Effects dial: r[2] bits 4..1 (high) + r[3] bit 7 (low 1 bit) -> 5-bit.
    uint8_t ef = (uint8_t)(((r[2] << 1) & 0x1E) | ((r[3] >> 7) & 0x01));
    out->analog[WII_AXIS_LT] = (uint16_t)(xf << 6);  // 4-bit -> 10-bit
    out->analog[WII_AXIS_RT] = (uint16_t)(ef << 5);  // 5-bit -> 10-bit

    // Buttons (active-low in r[4] and r[5]).
    uint16_t b45 = (uint16_t)((~r[4] & 0xFF) << 8) | (uint16_t)((~r[5]) & 0xFF);
    uint32_t btns = 0;
    if (b45 & (1u << 11)) btns |= WII_BTN_MINUS;            // byte 4 bit 3
    if (b45 & (1u << 10)) btns |= WII_BTN_PLUS;             // byte 4 bit 2
    if (b45 & (1u << 13)) btns |= WII_BTN_DJ_EUPHORIA;      // byte 4 bit 5
    if (b45 & (1u << 15)) btns |= WII_BTN_DJ_LEFT_GREEN;    // byte 4 bit 7
    if (b45 & (1u << 14)) btns |= WII_BTN_DJ_LEFT_RED;      // byte 4 bit 6
    if (b45 & (1u << 2))  btns |= WII_BTN_DJ_LEFT_BLUE;     // byte 5 bit 2 — LB
    if (b45 & (1u << 5))  btns |= WII_BTN_DJ_RIGHT_GREEN;   // byte 5 bit 5
    if (b45 & (1u << 1))  btns |= WII_BTN_DJ_RIGHT_RED;     // byte 5 bit 1
    if (b45 & (1u << 7))  btns |= WII_BTN_DJ_RIGHT_BLUE;    // byte 5 bit 7

    out->buttons = btns;
    out->has_accel = false;
    out->has_gyro = false;
}
