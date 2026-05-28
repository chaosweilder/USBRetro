// profiles.h - LodgeNet2N64 Profile Definitions
//
// Three profiles, auto-switched by app_task() based on detected controller:
//   Profile 0: N64 LodgeNet controller
//   Profile 1: GameCube LodgeNet controller
//   Profile 2: SNES LodgeNet controller

#ifndef LODGENET2N64_PROFILES_H
#define LODGENET2N64_PROFILES_H

#include "core/services/profiles/profile.h"
#include "native/device/n64/n64_buttons.h"

// ============================================================================
// PROFILE 0: N64 LodgeNet -> N64
// ============================================================================
// LodgeNet N64 input: A→B1, B→B3, C-Down→B2, C-Left→B4, C-Up→L3, C-Right→R3
//                     Z→R1, L→L2, R→R2, Start→S2
// N64 output expects: A=B1, B=B3, C-Down=B2, C-Left=B4, C-Up=R2, C-Right=R3
//                     Z=L2, L=L1, R=R1, Start=S2
// Remaps: Z(R1→L2), L(L2→L1), R(R2→R1), C-Up(L3→R2)

static const button_map_entry_t ln_n64_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, N64_BUTTON_A),      // A → A
    MAP_BUTTON(JP_BUTTON_B3, N64_BUTTON_B),      // B → B
    MAP_BUTTON(JP_BUTTON_B2, N64_BUTTON_CD),     // C-Down
    MAP_BUTTON(JP_BUTTON_B4, N64_BUTTON_CL),     // C-Left
    MAP_BUTTON(JP_BUTTON_L3, N64_BUTTON_CU),     // C-Up (L3→R2)
    MAP_BUTTON(JP_BUTTON_R3, N64_BUTTON_CR),     // C-Right
    MAP_BUTTON(JP_BUTTON_R1, N64_BUTTON_Z),      // Z (R1→L2)
    MAP_BUTTON(JP_BUTTON_L2, N64_BUTTON_L),      // L (L2→L1)
    MAP_BUTTON(JP_BUTTON_R2, N64_BUTTON_R),      // R (R2→R1)
    MAP_BUTTON(JP_BUTTON_S2, N64_BUTTON_START),  // Start
    // Menu → Z+R+A+B+Start (N64 reset/return combo)
    { .input = JP_BUTTON_A1, .output = (N64_BUTTON_Z | N64_BUTTON_R | N64_BUTTON_A | N64_BUTTON_B | N64_BUTTON_START), .analog = ANALOG_TARGET_NONE, .analog_value = 0 },
    MAP_DISABLED(JP_BUTTON_S1),
    MAP_DISABLED(JP_BUTTON_L1),
    MAP_DISABLED(JP_BUTTON_A2),
    MAP_DISABLED(JP_BUTTON_A4),
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_n64_profile = {
    .name = "n64",
    .description = "N64 LodgeNet controller",
    .button_map = ln_n64_map,
    .button_map_count = sizeof(ln_n64_map) / sizeof(ln_n64_map[0]),
    .l2_behavior = TRIGGER_PASSTHROUGH,
    .r2_behavior = TRIGGER_PASSTHROUGH,
    .l2_threshold = 128,
    .r2_threshold = 128,
    .l2_analog_value = 0,
    .r2_analog_value = 0,
    .left_stick_sensitivity = 1.0f,
    .right_stick_sensitivity = 1.0f,
    .left_stick_modifiers = NULL,
    .left_stick_modifier_count = 0,
    .right_stick_modifiers = NULL,
    .right_stick_modifier_count = 0,
    .adaptive_triggers = false,
    .socd_mode = SOCD_PASSTHROUGH,
};

// ============================================================================
// PROFILE 1: GC LodgeNet -> N64
// ============================================================================
// LodgeNet GC input: A→B2, B→B1, X→B4, Y→B3, Z→R1, L→L2, R→R2, Start→S2
//                    Main stick→LX/LY, C-stick→RX/RY, triggers→L2/R2 analog
// N64 output expects: A=B1, B=B3, Z=L2, L=L1, R=R1
// Mapping: A(B2)→N64 A, B(B1)→N64 B, Y(B3)→C-Left, X(B4)→C-Up
//          Z(R1)→Z, L(L2)→L, R(R2)→R
// C-stick (right analog) maps to C-buttons via threshold in n64_device.c

static const button_map_entry_t ln_gc_map[] = {
    MAP_BUTTON(JP_BUTTON_B2, N64_BUTTON_A),      // GC A → N64 A
    MAP_BUTTON(JP_BUTTON_B1, N64_BUTTON_B),      // GC B → N64 B
    MAP_BUTTON(JP_BUTTON_B3, N64_BUTTON_CL),     // GC Y → C-Left
    MAP_BUTTON(JP_BUTTON_B4, N64_BUTTON_CU),     // GC X → C-Up
    MAP_BUTTON(JP_BUTTON_R1, N64_BUTTON_Z),      // Z (R1→L2)
    MAP_BUTTON(JP_BUTTON_L2, N64_BUTTON_L),      // L (L2→L1)
    MAP_BUTTON(JP_BUTTON_R2, N64_BUTTON_R),      // R (R2→R1)
    MAP_BUTTON(JP_BUTTON_S2, N64_BUTTON_START),  // Start
    // Menu → Z+R+A+B+Start (N64 reset/return combo)
    { .input = JP_BUTTON_A1, .output = (N64_BUTTON_Z | N64_BUTTON_R | N64_BUTTON_A | N64_BUTTON_B | N64_BUTTON_START), .analog = ANALOG_TARGET_NONE, .analog_value = 0 },
    // C-stick → C-buttons handled by right stick threshold in n64_device.c
    MAP_DISABLED(JP_BUTTON_L1),
    MAP_DISABLED(JP_BUTTON_L3),
    MAP_DISABLED(JP_BUTTON_R3),
    MAP_DISABLED(JP_BUTTON_S1),
    MAP_DISABLED(JP_BUTTON_A2),
    MAP_DISABLED(JP_BUTTON_A4),
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_gc_profile = {
    .name = "gc",
    .description = "GameCube LodgeNet controller",
    .button_map = ln_gc_map,
    .button_map_count = sizeof(ln_gc_map) / sizeof(ln_gc_map[0]),
    .l2_behavior = TRIGGER_PASSTHROUGH,
    .r2_behavior = TRIGGER_PASSTHROUGH,
    .l2_threshold = 128,
    .r2_threshold = 128,
    .l2_analog_value = 0,
    .r2_analog_value = 0,
    .left_stick_sensitivity = 1.0f,
    .right_stick_sensitivity = 1.0f,
    .left_stick_modifiers = NULL,
    .left_stick_modifier_count = 0,
    .right_stick_modifiers = NULL,
    .right_stick_modifier_count = 0,
    .adaptive_triggers = false,
    .socd_mode = SOCD_PASSTHROUGH,
};

// ============================================================================
// PROFILE 2: SNES LodgeNet -> N64 (D-pad mode, default)
// ============================================================================
// D-pad → D-pad, L → L, Select → Z
// Order (A2) toggles to stick mode (profile 3)

static const button_map_entry_t ln_snes_dpad_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, N64_BUTTON_A),      // SNES B (B1) → N64 A
    MAP_BUTTON(JP_BUTTON_B2, N64_BUTTON_CD),     // SNES A (B2) → N64 C-Down
    MAP_BUTTON(JP_BUTTON_B3, N64_BUTTON_B),      // SNES Y (B3) → N64 B
    MAP_BUTTON(JP_BUTTON_B4, N64_BUTTON_CL),     // SNES X (B4) → N64 C-Left
    MAP_BUTTON(JP_BUTTON_L1, N64_BUTTON_L),      // L → L
    MAP_BUTTON(JP_BUTTON_R1, N64_BUTTON_R),      // R → R
    MAP_BUTTON(JP_BUTTON_S1, N64_BUTTON_Z),      // Select → Z
    MAP_BUTTON(JP_BUTTON_S2, N64_BUTTON_START),  // Start
    // Menu → Z+R+A+B+Start (N64 reset/return combo)
    { .input = JP_BUTTON_A1, .output = (N64_BUTTON_Z | N64_BUTTON_R | N64_BUTTON_A | N64_BUTTON_B | N64_BUTTON_START), .analog = ANALOG_TARGET_NONE, .analog_value = 0 },
    MAP_BUTTON(JP_BUTTON_A3, N64_BUTTON_CR),     // Minus (U+D SOCD) → C-Right
    MAP_BUTTON(JP_BUTTON_A4, N64_BUTTON_CU),     // Plus (L+R SOCD) → C-Up
    MAP_DISABLED(JP_BUTTON_L2),
    MAP_DISABLED(JP_BUTTON_R2),
    MAP_DISABLED(JP_BUTTON_L3),
    MAP_DISABLED(JP_BUTTON_R3),
    MAP_DISABLED(JP_BUTTON_A2),                   // Order — handled in app_task as toggle
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_snes_dpad_profile = {
    .name = "snes-dpad",
    .description = "SNES LodgeNet (D-pad mode)",
    .button_map = ln_snes_dpad_map,
    .button_map_count = sizeof(ln_snes_dpad_map) / sizeof(ln_snes_dpad_map[0]),
    .l2_behavior = TRIGGER_PASSTHROUGH,
    .r2_behavior = TRIGGER_PASSTHROUGH,
    .l2_threshold = 128,
    .r2_threshold = 128,
    .l2_analog_value = 0,
    .r2_analog_value = 0,
    .left_stick_sensitivity = 1.0f,
    .right_stick_sensitivity = 1.0f,
    .left_stick_modifiers = NULL,
    .left_stick_modifier_count = 0,
    .right_stick_modifiers = NULL,
    .right_stick_modifier_count = 0,
    .adaptive_triggers = false,
    .socd_mode = SOCD_PASSTHROUGH,
};

// ============================================================================
// PROFILE 3: SNES LodgeNet -> N64 (Stick mode)
// ============================================================================
// D-pad → Analog stick, L → Z, Select → L
// Order (A2) toggles back to d-pad mode (profile 2)

static const button_map_entry_t ln_snes_stick_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, N64_BUTTON_A),      // SNES B (B1) → N64 A
    MAP_BUTTON(JP_BUTTON_B2, N64_BUTTON_CD),     // SNES A (B2) → N64 C-Down
    MAP_BUTTON(JP_BUTTON_B3, N64_BUTTON_B),      // SNES Y (B3) → N64 B
    MAP_BUTTON(JP_BUTTON_B4, N64_BUTTON_CL),     // SNES X (B4) → N64 C-Left
    MAP_BUTTON(JP_BUTTON_L1, N64_BUTTON_Z),      // L → Z (swapped)
    MAP_BUTTON(JP_BUTTON_R1, N64_BUTTON_R),      // R → R
    MAP_BUTTON(JP_BUTTON_S1, N64_BUTTON_L),      // Select → L (swapped)
    MAP_BUTTON(JP_BUTTON_S2, N64_BUTTON_START),  // Start
    // Menu → Z+R+A+B+Start (N64 reset/return combo)
    { .input = JP_BUTTON_A1, .output = (N64_BUTTON_Z | N64_BUTTON_R | N64_BUTTON_A | N64_BUTTON_B | N64_BUTTON_START), .analog = ANALOG_TARGET_NONE, .analog_value = 0 },
    MAP_BUTTON(JP_BUTTON_A3, N64_BUTTON_CR),     // Minus (U+D SOCD) → C-Right
    MAP_BUTTON(JP_BUTTON_A4, N64_BUTTON_CU),     // Plus (L+R SOCD) → C-Up
    // D-pad → Analog stick
    MAP_BUTTON_ANALOG(JP_BUTTON_DU, 0, ANALOG_TARGET_LY_MIN, 0),    // Up → stick Y min (up)
    MAP_BUTTON_ANALOG(JP_BUTTON_DD, 0, ANALOG_TARGET_LY_MAX, 255),  // Down → stick Y max (down)
    MAP_BUTTON_ANALOG(JP_BUTTON_DL, 0, ANALOG_TARGET_LX_MIN, 0),    // Left → stick X min (left)
    MAP_BUTTON_ANALOG(JP_BUTTON_DR, 0, ANALOG_TARGET_LX_MAX, 255),  // Right → stick X max (right)
    MAP_DISABLED(JP_BUTTON_L2),
    MAP_DISABLED(JP_BUTTON_R2),
    MAP_DISABLED(JP_BUTTON_L3),
    MAP_DISABLED(JP_BUTTON_R3),
    MAP_DISABLED(JP_BUTTON_A2),                   // Order — handled in app_task as toggle
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_snes_stick_profile = {
    .name = "snes-stick",
    .description = "SNES LodgeNet (Stick mode)",
    .button_map = ln_snes_stick_map,
    .button_map_count = sizeof(ln_snes_stick_map) / sizeof(ln_snes_stick_map[0]),
    .l2_behavior = TRIGGER_PASSTHROUGH,
    .r2_behavior = TRIGGER_PASSTHROUGH,
    .l2_threshold = 128,
    .r2_threshold = 128,
    .l2_analog_value = 0,
    .r2_analog_value = 0,
    .left_stick_sensitivity = 1.0f,
    .right_stick_sensitivity = 1.0f,
    .left_stick_modifiers = NULL,
    .left_stick_modifier_count = 0,
    .right_stick_modifiers = NULL,
    .right_stick_modifier_count = 0,
    .adaptive_triggers = false,
    .socd_mode = SOCD_PASSTHROUGH,
};

// ============================================================================
// PROFILE SET — order matters: 0=N64, 1=GC, 2=SNES (matched by app_task)
// ============================================================================

static const profile_t lodgenet_n64_profiles[] = {
    ln_n64_profile,
    ln_gc_profile,
    ln_snes_dpad_profile,
    ln_snes_stick_profile,
};

static const profile_set_t n64_profile_set = {
    .profiles = lodgenet_n64_profiles,
    .profile_count = sizeof(lodgenet_n64_profiles) / sizeof(lodgenet_n64_profiles[0]),
    .default_index = 0,
};

#endif // LODGENET2N64_PROFILES_H
