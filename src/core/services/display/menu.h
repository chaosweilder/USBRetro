// menu.h - Tiny OLED menu system
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Robert Dale Smith
//
// Static-table menu for 128×64 mono displays. App declares menus as
// const arrays at compile time and wires up button input via menu_input().

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MENU_BTN_UP,        // A on FeatherWing OLED
    MENU_BTN_SELECT,    // B
    MENU_BTN_DOWN,      // C
    MENU_BTN_BACK,      // B held / longer press
} menu_button_t;

struct menu;

typedef struct menu_item {
    const char* label;
    // One of these is set to indicate the item's behavior:
    void (*action)(void);                // run a one-shot action
    const struct menu* submenu;          // descend into a submenu
    // Optional: returns a short string shown right-aligned (e.g. "[on]"
    // or current-setting marker). Pass NULL if no inline value.
    const char* (*get_value)(void);
} menu_item_t;

typedef struct menu {
    const char* title;
    const menu_item_t* items;
    uint8_t count;
} menu_t;

// Initialize / reset menu state to a root menu. Pass NULL to clear.
void menu_open(const menu_t* root);
void menu_close(void);

// True when a menu is currently active (caller's display loop should
// render the menu instead of its own screens).
bool menu_is_open(void);

// Feed a button press. Handles selection movement, submenu descent,
// action invocation, back-navigation, and root-level exit (closes the
// menu when BACK is pressed at the top).
void menu_input(menu_button_t btn);

// Render the current menu into the framebuffer. Caller is responsible
// for display_clear / display_update around this.
void menu_render(void);

#endif // MENU_H
