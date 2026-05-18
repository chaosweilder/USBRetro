// splash_image.h — bitmap-based splash for the gc2usb GBA payload.
// Replaces / augments the vector splash in splash.c with real PNG art
// converted to GBA Mode-3 (BGR555 direct-color) blits.
//
// Workflow:
//   1. Drop a PNG into  gba/joypad/assets/<name>.png
//   2. Run             gba/tools/png_to_splash.py
//   3. Reference       splash_img_<name> in splash_image_for()
//
// Mode-3 is single-page (no double buffer) and 16bpp, so the splash
// happens BEFORE display_init() puts us into Mode-4 for the eyes, or
// the renderer hot-swaps DISPCNT and we let display.c reclaim Mode-4
// when the splash hold ends.

#ifndef SPLASH_IMAGE_H
#define SPLASH_IMAGE_H

#include <stdint.h>
#include "mode_marker.h"

typedef struct {
    // 8bpp indexed pixels (1 byte per pixel, palette index).
    // Used when `palette` is non-NULL.
    const uint8_t*  pixels8;
    // 256-entry BGR555 palette for `pixels8`. NULL means use 16bpp pixels.
    const uint16_t* palette;
    // 16bpp BGR555 pixels — used when `palette` is NULL.
    const uint16_t* pixels;
    uint16_t        width;
    uint16_t        height;
    uint16_t        bg_color;  // BGR555 — fills any uncovered margin
} splash_image_t;

// Returns the image for `mode`, or NULL if no image is configured.
// NULL = caller falls back to splash.c's vector splash for that mode.
const splash_image_t* splash_image_for(joypad_mode_id_t mode);

// Switch the GBA to Mode-3 and blit `img` centered with bg_color fill.
// VRAM at 0x06000000 is written directly. Caller is responsible for
// returning to Mode-4 (set DISPCNT to 0x0004 | 0x0400) before resuming
// the eye animation.
void splash_image_render(const splash_image_t* img);

#endif
