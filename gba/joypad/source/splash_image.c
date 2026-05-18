// splash_image.c — implementation of the bitmap-blit splash renderer.

#include <string.h>
#include <stdbool.h>
#include <gba_video.h>
#include "splash_image.h"

// Per-mode image declarations — defined in splash_images/img_<mode>.c.
// Add a new declaration here AND an entry in splash_image_for() to
// wire a new image in. Order matches joypad_mode_id_t for readability,
// not enforcement.
#define DECL_IMG(sym) extern const splash_image_t sym
DECL_IMG(splash_img_switch);
DECL_IMG(splash_img_xinput);
DECL_IMG(splash_img_ps3);
DECL_IMG(splash_img_ps4);
DECL_IMG(splash_img_sinput);
DECL_IMG(splash_img_keyboard_mouse);
DECL_IMG(splash_img_gc_adapter);
DECL_IMG(splash_img_xbox_original);
DECL_IMG(splash_img_xbone);
#undef DECL_IMG

// Each img_<mode>.c declares `const splash_image_t splash_img_*` as a
// strong global. If the .c isn't compiled (e.g. nobody dropped a PNG
// for that mode), the link will fail — so wrap with a weak fallback
// definition here that returns NULL. We use __attribute__((weak)) on
// pointer accessors instead to avoid that complication: the lookup
// table is built with #if defined() against compile-time flags set
// by the Makefile when a PNG exists.
//
// For now the Makefile compiles every splash_images/*.c it finds, so
// only declare images that actually have asset files committed.

// Compile-time presence: if the .c file exists, the linker resolves
// the extern. Otherwise we get an undefined-reference error at link
// time. To avoid that for "image not yet authored" modes, this table
// only references images we actually have on disk. Edit alongside
// committing a new splash_images/img_<mode>.c file.
const splash_image_t* splash_image_for(joypad_mode_id_t mode)
{
    switch (mode) {
#ifdef HAVE_SPLASH_SWITCH
        case JOYPAD_MODE_SWITCH:          return &splash_img_switch;
#endif
#ifdef HAVE_SPLASH_XINPUT
        case JOYPAD_MODE_XINPUT:          return &splash_img_xinput;
#endif
#ifdef HAVE_SPLASH_PS3
        case JOYPAD_MODE_PS3:             return &splash_img_ps3;
#endif
#ifdef HAVE_SPLASH_PS4
        case JOYPAD_MODE_PS4:             return &splash_img_ps4;
#endif
#ifdef HAVE_SPLASH_SINPUT
        case JOYPAD_MODE_SINPUT:          return &splash_img_sinput;
#endif
#ifdef HAVE_SPLASH_KEYBOARD_MOUSE
        case JOYPAD_MODE_KEYBOARD_MOUSE:  return &splash_img_keyboard_mouse;
#endif
#ifdef HAVE_SPLASH_GC_ADAPTER
        case JOYPAD_MODE_GC_ADAPTER:      return &splash_img_gc_adapter;
#endif
#ifdef HAVE_SPLASH_XBOX_ORIGINAL
        case JOYPAD_MODE_XBOX_ORIGINAL:   return &splash_img_xbox_original;
#endif
#ifdef HAVE_SPLASH_XBONE
        case JOYPAD_MODE_XBONE:           return &splash_img_xbone;
#endif
        default: return NULL;
    }
}

void splash_image_render(const splash_image_t* img)
{
    if (!img) return;

    const bool indexed = (img->palette != NULL && img->pixels8 != NULL);
    const bool direct  = (img->pixels  != NULL);
    if (!indexed && !direct) return;

    int img_w = img->width, img_h = img->height;
    int x_off = (240 - img_w) / 2;
    int y_off = (160 - img_h) / 2;
    if (x_off < 0) x_off = 0;
    if (y_off < 0) y_off = 0;
    int copy_w = img_w; if (copy_w + x_off > 240) copy_w = 240 - x_off;
    int copy_h = img_h; if (copy_h + y_off > 160) copy_h = 160 - y_off;

    if (indexed) {
        // Mode-4: 240x160 8bpp indexed, double-page. Use page 0.
        REG_DISPCNT = 0x0004 | 0x0400;  // mode 4 + BG2 enable, front=page0

        // Load palette (256 entries × 2 bytes) → BG palette base.
        uint16_t* pal = (uint16_t*)0x05000000;
        for (int i = 0; i < 256; i++) pal[i] = img->palette[i];

        // VRAM in Mode 4 writes must be at least 16-bit. We pack pixel
        // pairs and write halfwords. Source 8bpp blit at x_off, y_off.
        uint16_t* vram = (uint16_t*)VRAM;
        const uint16_t bg_pair = ((uint16_t)0 << 8) | 0;  // index 0
        const int row_halfwords = 240 / 2;

        // Fill entire framebuffer with index 0 (which palette[0] should
        // be set to the desired bg_color by the converter).
        for (int i = 0; i < 240 * 160 / 2; i++) vram[i] = bg_pair;

        // Blit indexed image. To handle odd x_off, do byte-level via
        // 16-bit halfword reads of two src bytes at a time, but VRAM
        // forbids 8-bit writes — so read-modify-write halfwords.
        for (int y = 0; y < copy_h; y++) {
            const uint8_t* src = img->pixels8 + (int)y * img_w;
            int dst_y = y + y_off;
            int dst_x = x_off;
            for (int x = 0; x < copy_w; x++) {
                int vx = dst_x + x;
                int hw_idx = (dst_y * row_halfwords) + (vx >> 1);
                uint16_t cur = vram[hw_idx];
                if (vx & 1) {
                    cur = (cur & 0x00FF) | ((uint16_t)src[x] << 8);
                } else {
                    cur = (cur & 0xFF00) | (uint16_t)src[x];
                }
                vram[hw_idx] = cur;
            }
        }
        return;
    }

    // Direct-color (Mode-3) path — kept for backwards compatibility
    // with images that were converted before the 8bpp pipeline.
    REG_DISPCNT = 0x0003 | 0x0400;
    uint16_t* vram = (uint16_t*)VRAM;
    uint32_t bg32 = ((uint32_t)img->bg_color << 16) | img->bg_color;
    uint32_t* vram32 = (uint32_t*)vram;
    for (uint32_t i = 0; i < 240 * 160 / 2; i++) vram32[i] = bg32;
    for (int y = 0; y < copy_h; y++) {
        const uint16_t* src = img->pixels + (int)y * img_w;
        uint16_t* dst = vram + ((y + y_off) * 240) + x_off;
        for (int x = 0; x < copy_w; x++) dst[x] = src[x];
    }
}
