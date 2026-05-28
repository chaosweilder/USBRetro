// ext_drums.c - Rock Band / Guitar Hero drums report parser
//
// CAVEAT — ID collision: Rock Band drums report the same extension ID
// (`id[0]=0x01, id[5]=0x03`) as a Guitar Hero World Tour *guitar*.
// `wii_ext.c::classify()` defaults id[0]=0x01 to WII_EXT_TYPE_GUITAR,
// so this parser is never reached today from stock detection. Routing a
// real drum kit here requires either:
//   (a) a post-init report-shape check that sniffs the byte-2 "pad-id +
//       velocity" pattern drums emit and guitars don't, flipping ext->type
//       to WII_EXT_TYPE_DRUMS, or
//   (b) an app-level override that pins WII_EXT_TYPE_DRUMS on plug-in
//       (e.g., a dedicated `wii2usb_drums` target that forces the parser).
// When someone has a drum kit to test with, pick one of those paths.
// The parser itself is untested against real hardware — the byte layout
// below is from wiibrew but neither the pad velocity mapping nor the
// pad-id→index table have been cross-checked on a physical kit.
//
// 6-byte report layout (from wiibrew):
//   [0]      - - SX5..SX0                (joystick X, 6-bit — typically unused)
//   [1]      - - SY5..SY0                (joystick Y)
//   [2]      HHP HH3 HH2 HH1 HH0 - - -    (current velocity pad id + HHP=pedal flag)
//   [3]      - SH2 SH1 SH0 SH3 - - -     (4-bit velocity for the pad in [2])
//   [4]      1 1 BPedal 1 BMinus BPlus 1 1    (active-low)
//   [5]      BO BR BY BG BB - 1 1              (orange, red, yellow, green, blue frets; active-low)
//
// Only one pad's velocity is reported per poll; the digital which-pads-
// are-pressed bits in byte 5 are what the button fields surface every poll.
// The 4-bit velocity ends up in `state->pad_velocity[pad]`.

#include "wii_ext.h"

// Pad velocity address→index map. Pad ID in byte 2, bits 5..3.
//   0x12 = red   (WII_DRUM_PAD_RED)
//   0x13 = "drum hi-hat" = yellow cymbal
//   0x11 = green cymbal (high tom)  — maps to green for simplicity
//   0x0F = blue cymbal
//   0x0E = orange/bass? (varies by drum kit model)
// Mapping from byte 2 velocity-pad-id to our WII_DRUM_PAD_* index.
static int pad_id_to_index(uint8_t pad_id) {
    switch (pad_id) {
        case 0x12: return WII_DRUM_PAD_RED;
        case 0x13: return WII_DRUM_PAD_YELLOW;
        case 0x0F: return WII_DRUM_PAD_BLUE;
        case 0x11: return WII_DRUM_PAD_GREEN;
        case 0x0E: return WII_DRUM_PAD_ORANGE;
        case 0x1B: return WII_DRUM_PAD_BASS;
        default:   return -1;
    }
}

void wii_ext_parse_drums(wii_ext_t *ext, const uint8_t *r, wii_ext_state_t *out)
{
    (void)ext;

    // Joystick (if present on this drum kit).
    out->analog[WII_AXIS_LX] = (uint16_t)((r[0] & 0x3F) << 4);
    out->analog[WII_AXIS_LY] = (uint16_t)((r[1] & 0x3F) << 4);
    out->analog[WII_AXIS_RX] = 512;
    out->analog[WII_AXIS_RY] = 512;
    out->analog[WII_AXIS_LT] = 0;
    out->analog[WII_AXIS_RT] = 0;

    // Velocity reported for one pad per poll. Bits: pad_id = (r[2] >> 1) & 0x1F.
    // Velocity = 7 - ((r[3] >> 5) & 0x07) — wiibrew convention (higher = harder).
    uint8_t pad_id = (uint8_t)((r[2] >> 1) & 0x1F);
    uint8_t vel    = (uint8_t)(7 - ((r[3] >> 5) & 0x07));
    // "Hit" flag in r[2] bit 6 — 0 means currently reporting a hit.
    bool  velocity_valid = ((r[2] & 0x40) == 0);

    for (int i = 0; i < 6; i++) out->pad_velocity[i] = 0;
    if (velocity_valid) {
        int idx = pad_id_to_index(pad_id);
        if (idx >= 0) {
            // Scale 0..7 velocity up to 0..255 for downstream pressure consumers.
            out->pad_velocity[idx] = (uint8_t)((vel * 36) & 0xFF);
        }
    }

    // Button state — all digital, inverted in r[4] and r[5].
    uint16_t b45 = (uint16_t)((~r[4] & 0xFF) << 8) | (uint16_t)((~r[5]) & 0xFF);
    uint32_t btns = 0;
    if (b45 & (1u << 13)) btns |= WII_BTN_DRUM_BASS;      // byte 4 bit 5
    if (b45 & (1u << 11)) btns |= WII_BTN_MINUS;          // byte 4 bit 3
    if (b45 & (1u << 10)) btns |= WII_BTN_PLUS;           // byte 4 bit 2
    if (b45 & (1u << 7))  btns |= WII_BTN_DRUM_ORANGE;    // byte 5 bit 7
    if (b45 & (1u << 6))  btns |= WII_BTN_DRUM_RED;       // byte 5 bit 6
    if (b45 & (1u << 5))  btns |= WII_BTN_DRUM_YELLOW;    // byte 5 bit 5
    if (b45 & (1u << 4))  btns |= WII_BTN_DRUM_GREEN;     // byte 5 bit 4
    if (b45 & (1u << 3))  btns |= WII_BTN_DRUM_BLUE;      // byte 5 bit 3

    out->buttons = btns;
    out->has_accel = false;
    out->has_gyro = false;
}
