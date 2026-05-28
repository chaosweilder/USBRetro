// profiles.h - JVS2USB App Profiles
// SPDX-License-Identifier: Apache-2.0
//
// Profile definitions for JVS2USB adapter.

#ifndef JVS2USB_PROFILES_H
#define JVS2USB_PROFILES_H

#include "core/services/profiles/profile.h"

// ============================================================================
// BUTTON COMBOS
// ============================================================================

// S1 + S2 (alone) = A1 (Home/Guide button)
// Uses exclusive combo: only fires when S1+S2 are the ONLY buttons pressed
// If any other buttons are held (e.g., S1+S2+L1+R2), combo doesn't fire
// and all buttons pass through normally
static const button_combo_entry_t jvs2usb_combos[] = {
    MAP_COMBO_EXCLUSIVE(JP_BUTTON_S1 | JP_BUTTON_S2, JP_BUTTON_A1),
};

// ============================================================================
// DEFAULT PROFILE
// ============================================================================

static const profile_t jvs2usb_profiles[] = {
    {
        .name = "default",
        .description = "Standard mapping with Select+Start=Home",
        .button_map = NULL,
        .button_map_count = 0,
        .combo_map = jvs2usb_combos,
        .combo_map_count = sizeof(jvs2usb_combos) / sizeof(jvs2usb_combos[0]),
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
    },
};

// ============================================================================
// PROFILE SET
// ============================================================================

static const profile_set_t jvs2usb_profile_set = {
    .profiles = jvs2usb_profiles,
    .profile_count = sizeof(jvs2usb_profiles) / sizeof(jvs2usb_profiles[0]),
    .default_index = 0,
};

#endif // JVS2USB_PROFILES_H
