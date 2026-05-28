// ext_motionplus.c - Wii MotionPlus standalone report parser
//
// CAVEAT — PASSTHROUGH MODE NOT IMPLEMENTED.
// MotionPlus exposes two operating modes:
//   1. Standalone — the chip presents itself at 0x52 with its own
//      extension ID (`id[5]=0x05`) and returns a pure 6-byte gyro
//      report. **This parser handles that mode only.**
//   2. Passthrough — MotionPlus acts as a transparent proxy between
//      the Wiimote-facing bus (0x52) and a chained Nunchuck/Classic
//      plugged into its back. Reports alternate every other poll:
//      gyro-frame, extension-frame, gyro-frame, ... with a flag bit
//      in the last byte of each frame indicating which half is which.
//      Activation: write `0x04` to 0xFE on 0x53 (Nunchuck passthrough)
//      or `0x05` (Classic passthrough); the chip then re-identifies
//      with a distinct MP-passthrough ID at 0xFA. Once activated, a
//      single poll yields either six bytes of gyro or six bytes of
//      extension data — wii_ext_poll() would need to alternate the
//      parser it invokes based on the MP-frame flag.
// Adding passthrough requires: a passthrough activation step in
// wii_ext.c::wii_ext_start(), a per-poll frame-type demultiplexer in
// wii_ext_poll(), and a composite state that merges the extension's
// buttons/analogs with the MP's gyro. Roughly half a day's work once
// someone has the MP + an extension on hand to test with.
//
// MotionPlus on its own (not in passthrough mode) returns a 6-byte report
// containing three 14-bit gyroscope values plus per-axis fast/slow mode
// flags. Passthrough activation (MP + Nunchuck or MP + Classic chains) is
// deferred to a later phase — this parser handles MotionPlus only.
//
// Layout (from wiibrew):
//   [0]   YAW7..YAW0                         (yaw   low byte)
//   [1]   ROLL7..ROLL0                       (roll  low byte)
//   [2]   PITCH7..PITCH0                     (pitch low byte)
//   [3]   YAW13..YAW8   PITCH_SLOW  YAW_SLOW
//   [4]   ROLL13..ROLL8 EXT_CONN    ROLL_SLOW
//   [5]   PITCH13..PITCH8 0 0
//
// Slow-mode bits: when set, the axis uses the slow (±500 dps) range;
// otherwise fast (±2000 dps). We normalize everything to the fast range
// by multiplying slow-range readings by 4 — downstream consumers don't
// need to know which mode each axis is in.

#include "wii_ext.h"

static int16_t unpack_gyro(uint8_t lo, uint8_t hi6, bool slow) {
    int16_t raw = (int16_t)(((uint16_t)(hi6 & 0xFC) << 6) | lo);  // 14-bit, zero-centered ~0x1F7F
    raw -= 8192;   // recenter to signed zero
    if (slow) raw *= 4;
    return raw;
}

void wii_ext_parse_mplus(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    bool yaw_slow   = (r[3] & 0x02) != 0;
    bool pitch_slow = (r[3] & 0x01) != 0;
    bool roll_slow  = (r[4] & 0x02) != 0;

    out->gyro[0] = unpack_gyro(r[2], r[5], pitch_slow);   // pitch (X)
    out->gyro[1] = unpack_gyro(r[1], r[4], roll_slow);    // roll  (Y)
    out->gyro[2] = unpack_gyro(r[0], r[3], yaw_slow);     // yaw   (Z)
    out->has_gyro  = true;
    out->has_accel = false;

    // No buttons or analogs on standalone MotionPlus.
    out->buttons = 0;
    out->analog[WII_AXIS_LX] = 512;
    out->analog[WII_AXIS_LY] = 512;
    out->analog[WII_AXIS_RX] = 512;
    out->analog[WII_AXIS_RY] = 512;
    out->analog[WII_AXIS_LT] = 0;
    out->analog[WII_AXIS_RT] = 0;
}
