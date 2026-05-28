# wii2usb

Wii extension accessories (Nunchuck, Classic Controller, Classic Pro, Guitar, Drums, Turntable, Taiko, uDraw, MotionPlus) to USB HID gamepad.

## Overview

Plug a Wii extension cable — or any Qwiic adapter that exposes the extension connector — into a KB2040 and the device enumerates as a USB gamepad. The extension is auto-detected on insert and the appropriate button / stick / analog mapping is applied automatically.

## Input

[Wii Extension Input](../input/wii.md) — I²C at 0x52 via the KB2040's STEMMA QT port (GP12/GP13).

## Output

[USB Device](../output/usb-device.md) — enumerates as whichever USB mode is currently active (SInput default; Xbox Original, XInput, PS3, PS4, Switch, PS Classic, Xbox One, XAC all selectable via the web config).

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (single player, 1:1 pass-through) |
| Player slots | 1 |
| I²C bus | Hardware I²C0 on GP12/GP13 @ 50 kHz |
| Profile system | Default passthrough profile (additional profiles can be added in `apps/wii2usb/profiles.h`) |

## Key Features

- **Auto-detect**: plug any Wii extension and the right parser runs — no app-level configuration needed.
- **Hot-plug**: unplug and plug back in; the device re-runs init on the next tick (~250 ms).
- **NeoPixel status**: dim white blink while scanning, solid cyan for Nunchuck, solid blue for Classic/Pro, off on disconnect.
- **Profile cycle hotkey**: hold **−** (Minus) + **D-Pad Up** for 2 s to cycle to the next profile (or + **D-Pad Down** for previous). LED animates to confirm.
- **Motion passthrough**: Nunchuck accelerometer and standalone MotionPlus gyro flow through `input_event_t.accel[]` / `gyro[]` into the SInput motion fields unchanged.
- **Drum velocity**: 4-bit per-pad velocity populates `event.pressure[]` — PS3 output mode carries it through for rhythm games.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make wii2usb_kb2040` |

## Build and Flash

```bash
make wii2usb_kb2040
make flash-wii2usb_kb2040    # drag-and-drop UF2 to /Volumes/RPI-RP2
```

## Wiring

See the [Wii Extension Input](../input/wii.md#wiring) section. Qwiic cable + Adafruit 4836 breakout + a Wii extension plugged into the breakout is the zero-soldering path. Direct-wire a cut Wii extension cable for a more permanent build.

## Hardware notes

The KB2040's STEMMA QT port is the easiest wiring option — it provides 3V3 power, GND, SDA (GP12), and SCL (GP13) on a single JST-SH connector. **Do not power Wii extensions from 5 V.**
