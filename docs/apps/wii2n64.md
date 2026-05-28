# wii2n64

Wii extension accessories (Nunchuck, Classic Controller, Classic Pro, etc.) to N64 console.

## Overview

Drive a real N64 with Wii accessories. Classic Controller buttons and sticks are particularly well suited — the 4-face + L/R/Z, D-pad, Start mapping gives you everything an N64 cart expects, with the left stick driving the analog thumb-stick and the right stick unused (N64 has none) or remappable for C-buttons via a future profile.

## Input

[Wii Extension Input](../input/wii.md) — I²C at 0x52 on the Pico.

## Output

[N64 Output](../output/n64.md) — single-wire joybus via RP2040 PIO.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE |
| Player slots | 1 |
| Clock speed | default (N64 joybus works at stock 125 MHz) |
| I²C bus | GP4 (SDA) / GP5 (SCL) @ 50 kHz |
| N64 data pin | GP2 |
| NeoPixel | disabled (plain Pico has no onboard WS2812) |
| Profile system | Default passthrough |

## Supported Boards

| Board | Build Command |
|-------|---------------|
| Pi Pico | `make wii2n64_pico` |

## Build and Flash

```bash
make wii2n64_pico
make flash-wii2n64_pico
```

## Wiring

The Pi Pico has no STEMMA QT port, so wire directly (or via an Adafruit Wii Nunchuck breakout with jumper wires to the Pico's edge pins):

| Pi Pico pin | Pin # | Role |
|-------------|-------|------|
| GP4 | 6 | Wii SDA |
| GP5 | 7 | Wii SCL |
| 3V3(OUT) | 36 | Wii extension power |
| GND | 38 (or any) | shared ground |
| GP2 | 4 | N64 data (joybus) |
| GND | — | N64 shield/ground |
| GP0 | 1 | UART TX (debug; optional) |
| GP1 | 2 | UART RX (debug; optional) |

**Do not power the Wii extension from 5 V.** All accessories are 3.3 V-only parts.

## Limitations

- Single N64 port — no multitap support.
- N64 has no right analog stick; right stick data from Classic / Pro is currently ignored. A future profile could remap the right stick to C-buttons (Up/Down/Left/Right) for titles that use them for camera control.
- Rumble is not forwarded (N64 rumble packs are per-game memory-card-like; the Wii accessories don't have rumble motors to drive anyway).
