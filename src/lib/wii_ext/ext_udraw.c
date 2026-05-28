// ext_udraw.c - THQ uDraw tablet report parser (best-effort)
//
// CAVEAT — UNTESTED against real hardware; layout is community-reversed.
// There is no authoritative Nintendo or THQ source for the uDraw report
// format. The specifics below are a synthesis of forum posts and the
// various Arduino/AVR references, none of which agree exactly. Expect
// to tune when a physical tablet is available:
//   - X/Y max ranges (using 1920 × 1200 as rough bounds — real tablet
//     may range differently; the scale factors in the LX/LY mapping
//     below will need adjusting once measured).
//   - X/Y nibble-pack order in r[2] (which half is X-high vs Y-high may
//     be inverted from what we assume).
//   - Pressure threshold for `tablet_active` — 0 is a safe bet, but
//     real units may report small idle pressure values while the pen
//     is merely hovering rather than touching.
//   - Button bit positions in r[4]/r[5] — uDraw has physical pen buttons
//     that we best-guess map to bits 0/1.
//
// Community-reverse-engineered layout (no authoritative Nintendo source):
//   [0] X low 8 bits
//   [1] Y low 8 bits
//   [2] XXXXYYYY        (X high 4 bits | Y high 4 bits)
//   [3] pressure (0..255)
//   [4] buttons 1       (-/+)
//   [5] buttons 2       (pen buttons; typically unused in Wii-side games)
//
// X range ~0..1919, Y range ~0..1199 (coarse approximation — varies per
// unit). We surface absolute position via input_event_t.touch[0] and
// pressure via analog[WII_AXIS_LT] so the router / output path can treat
// it as either touch or analog.

#include "wii_ext.h"

void wii_ext_parse_udraw(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    uint16_t x = (uint16_t)(r[0] | ((r[2] >> 4) & 0x0F) << 8);
    uint16_t y = (uint16_t)(r[1] | ((r[2] & 0x0F)) << 8);
    uint8_t  p = r[3];
    bool     active = (p > 0);

    out->tablet_x        = x;
    out->tablet_y        = y;
    out->tablet_pressure = p;
    out->tablet_active   = active;

    // Also expose position as sticks for consumers that don't look at
    // tablet_x/y — scale each axis to 0..1023.
    // X 0..1919 ≈ /2 then scale; Y 0..1199 ≈ *0.85 approx.
    out->analog[WII_AXIS_LX] = x > 1920 ? 1023 : (uint16_t)((uint32_t)x * 1023u / 1920u);
    out->analog[WII_AXIS_LY] = y > 1200 ? 1023 : (uint16_t)((uint32_t)y * 1023u / 1200u);
    out->analog[WII_AXIS_RX] = 512;
    out->analog[WII_AXIS_RY] = 512;
    out->analog[WII_AXIS_LT] = (uint16_t)p << 2;
    out->analog[WII_AXIS_RT] = 0;

    // Buttons (approximate — actual mapping varies by uDraw firmware).
    uint16_t b45 = (uint16_t)((~r[4] & 0xFF) << 8) | (uint16_t)((~r[5]) & 0xFF);
    uint32_t btns = 0;
    if (b45 & (1u << 11)) btns |= WII_BTN_MINUS;
    if (b45 & (1u << 10)) btns |= WII_BTN_PLUS;
    if (b45 & (1u <<  0)) btns |= WII_BTN_A;   // primary pen button
    if (b45 & (1u <<  1)) btns |= WII_BTN_B;   // secondary pen button
    out->buttons = btns;

    out->has_accel = false;
    out->has_gyro = false;
}
