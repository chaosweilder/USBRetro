// profiles.h - WII2GC App Profiles
// SPDX-License-Identifier: Apache-2.0
//
// Minimal profile set for wii2gc. The Wii Classic / Classic Pro mapping
// happens in the wii_host driver (A->B2, B->B1, X->B4, Y->B3), so the GC
// output just needs straight passthrough from the router's normalized event.

#ifndef WII2GC_PROFILES_H
#define WII2GC_PROFILES_H

#include "core/services/profiles/profile.h"

static const profile_t wii2gc_profiles[] = {
    {
        .name = "default",
        .description = "Wii passthrough to GameCube",
        .button_map = NULL,
        .button_map_count = 0,
        .combo_map = NULL,
        .combo_map_count = 0,
        PROFILE_TRIGGERS_DEFAULT,
        PROFILE_ANALOG_DEFAULT,
        .adaptive_triggers = false,
    },
};

static const profile_set_t wii2gc_profile_set = {
    .profiles = wii2gc_profiles,
    .profile_count = sizeof(wii2gc_profiles) / sizeof(wii2gc_profiles[0]),
    .default_index = 0,
};

#endif // WII2GC_PROFILES_H
