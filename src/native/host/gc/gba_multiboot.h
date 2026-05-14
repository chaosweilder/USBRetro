// gba_multiboot.h - GBA joybus multiboot uploader
//
// Pushes a .gba ROM into a GBA over the GC↔GBA link cable. After upload,
// the GBA runs the payload from 0x02000000. If the payload is the
// gba-as-controller stub, it then responds to standard GameCube poll
// commands and the existing GamecubeController_Poll path takes over with
// no further changes.
//
// Protocol implements the Kawasedo handshake + stream cipher used by
// Nintendo's GameCube BIOS / libogc JOYBOOT(). Reference: AxioDL/jbus
// (Endpoint.cpp) and GBATEK SIO JOY BUS Mode.

#ifndef GBA_MULTIBOOT_H
#define GBA_MULTIBOOT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "joybus.h"

typedef enum {
    GBA_MB_OK              =  0,
    GBA_MB_ERR_PROBE       = -1,  // probe failed / not a GBA
    GBA_MB_ERR_RESET       = -2,  // RESET/STATUS state mismatch
    GBA_MB_ERR_TRANSFER    = -3,  // JOYSTAT validation failed mid-upload
    GBA_MB_ERR_FINALIZE    = -4,  // post-upload boot poll/CRC echo failed
    GBA_MB_ERR_PARAMS      = -5,  // bad length / palette / speed
    GBA_MB_ERR_NO_PAYLOAD  = -6,  // no GBA payload linked into firmware
} gba_mb_result_t;

// Probe a joybus port: returns true if the device ID matches GBA Type 0x0400.
bool gba_mb_detect(joybus_port_t* port);

// Upload a .gba ROM and trigger GBA boot.
//   rom/len  : .gba file contents (1..256KB-1, padded internally to mult. of 8)
//   palette  : boot-logo color, 0..6
//   speed    : boot-logo speed, -4..+4
//   channel  : SI channel 0..3 (written into rom[0xC4] for the GBA's awareness)
gba_mb_result_t gba_mb_upload(joybus_port_t* port,
                              const uint8_t* rom, uint32_t len,
                              int palette, int speed, int channel);

// Detect + upload the firmware-embedded payload in one call.
// Returns GBA_MB_ERR_NO_PAYLOAD if no payload is linked (default stub).
gba_mb_result_t gba_mb_boot_embedded(joybus_port_t* port, int channel);

// After multiboot, READ 4 bytes of input data from the GBA-side payload.
// The payload uses joybus mode (READ command 0x14) — Doridian-style:
//   data[0] bit 0..7 = GBA KEYINPUT lower 8 bits (A, B, Select, Start, R, L, U, D)
//   data[1] bit 0..1 = GBA KEYINPUT upper bits (R, L)
// 0 bit = button pressed (matches GBA hardware convention).
// Returns 0 on success, negative on error.
int gba_input_read(joybus_port_t* port, uint8_t out[4]);

// Embedded GBA-as-controller payload. Defined weakly in gba_payload.c
// so the firmware links cleanly even when no .gba is provided. Override
// by replacing gba_payload.c with a generated source containing the
// actual binary.
extern const uint8_t  gba_payload[];
extern const uint32_t gba_payload_len;

#endif
