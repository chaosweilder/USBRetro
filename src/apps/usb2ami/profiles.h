// profiles.h - USB2AMI Button Mapping Profiles
//
// Profile 1 (Default): Standard CD32 layout — pass-through, no remapping
// Profile 2: Swapped Fire 1 (Red/B2) and Fire 2 (Blue/B1) for games that
//            use Red button as primary fire

#pragma once

#include "core/services/profiles/profile.h"
#include "core/buttons.h"

// Profile 1: Standard CD32 layout
static const profile_t ami_profile_standard = {
    .name             = "CD32 Standard",
    .button_map       = NULL,
    .button_map_count = 0,
};

// Profile 2: Swapped Fire 1/2
static const button_map_entry_t ami_swapped_map[] = {
    { .input = JP_BUTTON_B1, .output = JP_BUTTON_B2 },  // South -> Red (Fire 1)
    { .input = JP_BUTTON_B2, .output = JP_BUTTON_B1 },  // East  -> Blue (Fire 2)
};

static const profile_t ami_profile_swapped = {
    .name             = "CD32 Swapped",
    .button_map       = ami_swapped_map,
    .button_map_count = 2,
};

// Profile array and set
static const profile_t ami_profiles[] = {
    ami_profile_standard,
    ami_profile_swapped,
};

static const profile_set_t ami_profile_set = {
    .profiles      = ami_profiles,
    .profile_count = sizeof(ami_profiles) / sizeof(ami_profiles[0]),
    .default_index = 0,
};
