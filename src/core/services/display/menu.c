// menu.c - Tiny OLED menu system
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Robert Dale Smith

#include "menu.h"
#include "display.h"
#include <string.h>

// Layout for 128×64 with the built-in 6×8 font (display_text):
//   y= 0..7   title
//   y= 9      h-line under title
//   y=12..19  item 0 (4 visible rows, 10px stride)
//   y=22..29  item 1
//   y=32..39  item 2
//   y=42..49  item 3
//   y=51      h-line above hint
//   y=54..61  hint bar
#define ROW_TITLE_Y      0
#define ROW_DIVIDER_TOP  9
#define ROW_FIRST_Y      12
#define ROW_STRIDE       10
#define ROW_VISIBLE      4
#define ROW_DIVIDER_BOT  51
#define ROW_HINT_Y       54
#define COL_LABEL_X      8     // leaves room for the "> " selection arrow
#define COL_VALUE_RIGHT  126   // right edge for value text

#define STACK_DEPTH      4

typedef struct {
    const menu_t* menu;
    uint8_t selection;
    uint8_t scroll;     // index of first visible row
} menu_frame_t;

static menu_frame_t stack[STACK_DEPTH];
static int stack_top = -1;     // -1 = menu closed

static menu_frame_t* current(void) {
    return (stack_top >= 0) ? &stack[stack_top] : NULL;
}

void menu_open(const menu_t* root) {
    if (!root) {
        stack_top = -1;
        return;
    }
    stack_top = 0;
    stack[0].menu = root;
    stack[0].selection = 0;
    stack[0].scroll = 0;
}

void menu_close(void) {
    stack_top = -1;
}

bool menu_is_open(void) {
    return stack_top >= 0;
}

static void push(const menu_t* m) {
    if (stack_top + 1 >= STACK_DEPTH) return;   // would overflow
    stack_top++;
    stack[stack_top].menu = m;
    stack[stack_top].selection = 0;
    stack[stack_top].scroll = 0;
}

static void pop_or_close(void) {
    if (stack_top > 0) {
        stack_top--;
    } else {
        stack_top = -1;
    }
}

static void scroll_into_view(menu_frame_t* f) {
    if (f->selection < f->scroll) {
        f->scroll = f->selection;
    } else if (f->selection >= f->scroll + ROW_VISIBLE) {
        f->scroll = f->selection - (ROW_VISIBLE - 1);
    }
}

void menu_input(menu_button_t btn) {
    menu_frame_t* f = current();
    if (!f) return;
    const menu_t* m = f->menu;

    switch (btn) {
        case MENU_BTN_UP:
            if (m->count == 0) break;
            f->selection = (f->selection == 0) ? (m->count - 1) : (f->selection - 1);
            scroll_into_view(f);
            break;

        case MENU_BTN_DOWN:
            if (m->count == 0) break;
            f->selection = (f->selection + 1) % m->count;
            scroll_into_view(f);
            break;

        case MENU_BTN_SELECT: {
            if (f->selection >= m->count) break;
            const menu_item_t* item = &m->items[f->selection];
            if (item->submenu) {
                push(item->submenu);
            } else if (item->action) {
                item->action();
            }
            break;
        }

        case MENU_BTN_BACK:
            pop_or_close();
            break;
    }
}

void menu_render(void) {
    menu_frame_t* f = current();
    if (!f) return;
    const menu_t* m = f->menu;

    display_clear();

    // Title
    if (m->title) {
        display_text(0, ROW_TITLE_Y, m->title);
    }
    display_hline(0, ROW_DIVIDER_TOP, DISPLAY_WIDTH);

    // Visible items
    for (uint8_t row = 0; row < ROW_VISIBLE; row++) {
        uint8_t idx = f->scroll + row;
        if (idx >= m->count) break;
        uint8_t y = ROW_FIRST_Y + row * ROW_STRIDE;
        const menu_item_t* item = &m->items[idx];

        if (idx == f->selection) {
            display_text(0, y, ">");
        }
        if (item->label) {
            display_text(COL_LABEL_X, y, item->label);
        }
        if (item->get_value) {
            const char* v = item->get_value();
            if (v && v[0]) {
                // Right-align: 6 px per char in the built-in font.
                int len = (int)strlen(v);
                int x = COL_VALUE_RIGHT - len * 6;
                if (x < COL_LABEL_X) x = COL_LABEL_X;
                display_text((uint8_t)x, y, v);
            }
        }
    }

    // Hint bar
    display_hline(0, ROW_DIVIDER_BOT, DISPLAY_WIDTH);
    display_text(0, ROW_HINT_Y, "A:up  B:sel  C:dn");
}
