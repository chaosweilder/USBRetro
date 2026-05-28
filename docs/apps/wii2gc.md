# wii2gc

Wii extension accessories (Nunchuck, Classic Controller, Classic Pro, etc.) to GameCube console.

## Overview

Drive a real GameCube with a Wii Classic Controller, Classic Pro, Nunchuck, or any other Wii extension. Classic Pro is a particularly natural fit — the AXBY face, analog L/R triggers, 2 sticks, D-pad, +/-/Home layout maps 1:1 to GameCube.

## Input

[Wii Extension Input](../input/wii.md) — I²C at 0x52 on GP12/GP13.

## Output

[GameCube Output](../output/gamecube.md) — single-wire joybus via RP2040 PIO.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE |
| Player slots | 1 |
| Clock speed | 130 MHz (required for joybus timing) |
| I²C bus | GP12 / GP13 @ 50 kHz |
| GameCube data pin | GP7 (default) |
| GameCube 3V3 detect | GP6 |
| Profile system | Default passthrough |

## Modes

- **Play mode**: when GameCube 3V3 is detected on GP6, the adapter runs as a joybus peripheral feeding the GameCube.
- **Config mode**: when no GameCube 3V3 is detected, the adapter boots as a USB CDC device so the web config tool can read/write profiles and settings.

## Key Features

- Classic Pro → GameCube face / trigger / stick layout is direct (no remapping needed).
- Shared LED + profile-cycle hotkey behaviour with wii2usb (see that app's doc).
- Nunchuck-only or Classic-only operation works — buttons/axes the GC expects but the accessory doesn't provide are held at neutral.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make wii2gc_kb2040` |

## Build and Flash

```bash
make wii2gc_kb2040
make flash-wii2gc_kb2040
```

## Wiring

Combines Wii-side and GameCube-side wiring on the same KB2040:

| Pin | Role |
|-----|------|
| GP12 | Wii SDA (STEMMA QT) |
| GP13 | Wii SCL (STEMMA QT) |
| GP6 | GameCube 3V3 detect |
| GP7 | GameCube data (joybus) |
| 3V3 | Wii extension power |
| GND | shared ground |

## Limitations

- Single GameCube port for now; 4-port multitap support is a future addition.
- Nunchuck accelerometer and MotionPlus gyro are populated in `input_event_t` but the GameCube output doesn't carry motion data.
