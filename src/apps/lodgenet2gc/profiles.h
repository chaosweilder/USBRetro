// profiles.h - LodgeNet2GC Profile Definitions
//
// Three profiles, auto-switched by app_task() based on detected controller:
//   Profile 0: GameCube LodgeNet controller (1:1 passthrough)
//   Profile 1: N64 LodgeNet controller
//   Profile 2: SNES LodgeNet controller

#ifndef LODGENET2GC_PROFILES_H
#define LODGENET2GC_PROFILES_H

#include "core/services/profiles/profile.h"
#include "native/device/gamecube/gamecube_buttons.h"

// ============================================================================
// PROFILE 0: GC LodgeNet -> GC (1:1 passthrough)
// ============================================================================
// LodgeNet GC input: A→B2, B→B1, X→B4, Y→B3, Z→R1, L→L2, R→R2, Start→S2
// GC output aliases:  A=B2, B=B1, X=B4, Y=B3, Z=R1, L=L2, R=R2, Start=S2
// All match — true 1:1 passthrough.

static const button_map_entry_t ln_gc_gc_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, GC_BUTTON_B),
    MAP_BUTTON(JP_BUTTON_B2, GC_BUTTON_A),
    MAP_BUTTON(JP_BUTTON_B3, GC_BUTTON_Y),
    MAP_BUTTON(JP_BUTTON_B4, GC_BUTTON_X),
    MAP_BUTTON(JP_BUTTON_R1, GC_BUTTON_Z),
    MAP_BUTTON(JP_BUTTON_L2, GC_BUTTON_L),
    MAP_BUTTON(JP_BUTTON_R2, GC_BUTTON_R),
    MAP_BUTTON(JP_BUTTON_S2, GC_BUTTON_START),
    MAP_BUTTON(JP_BUTTON_A1, GC_BUTTON_START),  // Menu → Start
    MAP_DISABLED(JP_BUTTON_L1),
    MAP_DISABLED(JP_BUTTON_L3),
    MAP_DISABLED(JP_BUTTON_R3),
    MAP_DISABLED(JP_BUTTON_S1),
    MAP_DISABLED(JP_BUTTON_A2),
    MAP_DISABLED(JP_BUTTON_A4),
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_gc_gc_profile = {
    .name = "gc",
    .description = "GameCube LodgeNet controller",
    .button_map = ln_gc_gc_map,
    .button_map_count = sizeof(ln_gc_gc_map) / sizeof(ln_gc_gc_map[0]),
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
// PROFILE 1: N64 LodgeNet -> GC
// ============================================================================
// LodgeNet N64 input: A→B1, B→B3, C-Down→B2, C-Left→B4, C-Up→L3, C-Right→R3
//                     Z→R1, L→L2, R→R2, Start→S2
// Mapping: A(B1)→GC A, B(B3)→GC B, Z(R1)→GC Z, L(L2)→GC L, R(R2)→GC R
//          C-Down(B2)→GC X, C-Left(B4)→GC Y, C-Up(L3)→GC X (alt), C-Right(R3)→GC Y (alt)

static const button_map_entry_t ln_n64_gc_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, GC_BUTTON_A),      // N64 A → GC A
    MAP_BUTTON(JP_BUTTON_B3, GC_BUTTON_B),      // N64 B → GC B
    MAP_BUTTON(JP_BUTTON_B2, GC_BUTTON_X),      // C-Down → GC X
    MAP_BUTTON(JP_BUTTON_B4, GC_BUTTON_Y),      // C-Left → GC Y
    MAP_BUTTON(JP_BUTTON_R1, GC_BUTTON_Z),      // Z → Z
    MAP_BUTTON(JP_BUTTON_L2, GC_BUTTON_L),      // L → L
    MAP_BUTTON(JP_BUTTON_R2, GC_BUTTON_R),      // R → R
    MAP_BUTTON(JP_BUTTON_S2, GC_BUTTON_START),  // Start
    MAP_BUTTON(JP_BUTTON_A1, GC_BUTTON_START),  // Menu → Start
    MAP_DISABLED(JP_BUTTON_L1),
    MAP_DISABLED(JP_BUTTON_L3),   // C-Up (no good GC equivalent as button)
    MAP_DISABLED(JP_BUTTON_R3),   // C-Right
    MAP_DISABLED(JP_BUTTON_S1),
    MAP_DISABLED(JP_BUTTON_A2),
    MAP_DISABLED(JP_BUTTON_A4),
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_n64_gc_profile = {
    .name = "n64",
    .description = "N64 LodgeNet controller",
    .button_map = ln_n64_gc_map,
    .button_map_count = sizeof(ln_n64_gc_map) / sizeof(ln_n64_gc_map[0]),
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
// PROFILE 2: SNES LodgeNet -> GC
// ============================================================================
// LodgeNet SNES input: B→B1, A→B2, Y→B3, X→B4, L→L1, R→R1
//                      Select→S1, Start→S2
// Mapping: A(B2)→GC A, B(B1)→GC B, X(B4)→GC X, Y(B3)→GC Y
//          L(L1)→GC L, R(R1)→GC R, Select(S1)→GC Z

static const button_map_entry_t ln_snes_gc_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, GC_BUTTON_B),      // SNES B (B1) → GC B (same as GC profile)
    MAP_BUTTON(JP_BUTTON_B2, GC_BUTTON_A),      // SNES A (B2) → GC A
    MAP_BUTTON(JP_BUTTON_B3, GC_BUTTON_Y),      // SNES Y (B3) → GC Y
    MAP_BUTTON(JP_BUTTON_B4, GC_BUTTON_X),      // SNES X (B4) → GC X
    MAP_BUTTON(JP_BUTTON_L1, GC_BUTTON_L),      // L → L
    MAP_BUTTON(JP_BUTTON_R1, GC_BUTTON_R),      // R → R
    MAP_BUTTON(JP_BUTTON_S1, GC_BUTTON_Z),      // Select → Z
    MAP_BUTTON(JP_BUTTON_S2, GC_BUTTON_START),  // Start
    MAP_BUTTON(JP_BUTTON_A1, GC_BUTTON_START),  // Menu → Start
    MAP_DISABLED(JP_BUTTON_L2),
    MAP_DISABLED(JP_BUTTON_R2),
    MAP_DISABLED(JP_BUTTON_L3),
    MAP_DISABLED(JP_BUTTON_R3),
    MAP_DISABLED(JP_BUTTON_A2),
    MAP_DISABLED(JP_BUTTON_A3),
    MAP_DISABLED(JP_BUTTON_A4),
    MAP_DISABLED(JP_BUTTON_L4),
    MAP_DISABLED(JP_BUTTON_R4),
};

static const profile_t ln_snes_gc_profile = {
    .name = "snes",
    .description = "SNES LodgeNet controller",
    .button_map = ln_snes_gc_map,
    .button_map_count = sizeof(ln_snes_gc_map) / sizeof(ln_snes_gc_map[0]),
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
// PROFILE SET — order matters: 0=GC, 1=N64, 2=SNES (matched by app_task)
// ============================================================================

static const profile_t lodgenet_gc_profiles[] = {
    ln_gc_gc_profile,
    ln_n64_gc_profile,
    ln_snes_gc_profile,
};

static const profile_set_t gc_profile_set = {
    .profiles = lodgenet_gc_profiles,
    .profile_count = sizeof(lodgenet_gc_profiles) / sizeof(lodgenet_gc_profiles[0]),
    .default_index = 0,
};

#endif // LODGENET2GC_PROFILES_H
