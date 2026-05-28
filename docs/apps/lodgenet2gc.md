# lodgenet2gc

LodgeNet hotel gaming controller to GameCube console.

## Overview

Reads Nintendo LodgeNet hotel controllers via a proprietary 3-wire serial protocol and outputs to a GameCube console via the joybus protocol. A cross-console bridge -- no USB involved on either side. Automatically detects N64, GameCube, and SNES LodgeNet controller variants with hot-swap support. The GameCube variant maps 1:1 (no remapping needed); N64 and SNES variants are mapped naturally to GC buttons.

## Input

[LodgeNet Input](../input/lodgenet.md) -- PIO-based 3-wire serial protocol. MCU protocol for N64/GC (~60Hz), SR protocol for SNES (~131Hz). Auto-detection cycles between protocols.

## Output

[GameCube Output](../output/gamecube.md) -- Joybus PIO protocol on GPIO 7. Requires 130MHz overclock for correct timing.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | MERGE |
| Player slots | 1 (fixed) |
| LodgeNet Clock pin | GPIO 3 |
| LodgeNet Data pin | GPIO 2 |
| LodgeNet VCC pin | GPIO 4 |
| LodgeNet Clock2 pin | GPIO 5 (SNES SR protocol) |
| GC Data pin | GPIO 7 |
| Clock | 130MHz (overclock for joybus timing) |
| Connector (input) | RJ11 6P6C |
| Connector (output) | GameCube controller port |

## Key Features

- **Auto-detection** -- N64, GameCube, and SNES LodgeNet controllers detected automatically.
- **Hot-swap** -- Switch between controller types without rebooting.
- **Full analog** -- GC sticks, C-stick, and analog L/R triggers mapped directly.
- **Profile system** -- Default passthrough profile (LodgeNet GC buttons map 1:1 to GC output).
- **LED indicator** -- Slow blink: no LodgeNet controller. Solid: connected.
- **Bootloader reboot** -- Send 'B' via serial to reboot into UF2 bootloader.

## PIO Allocation

| PIO Block | Usage |
|-----------|-------|
| PIO0 | GameCube joybus device (Core 1, timing-critical) |
| PIO1 | LodgeNet host (MCU/SR programs, swapped at runtime) |

## Button Mapping

The default profile passes LodgeNet GC input directly to GC output (1:1 match):

| LodgeNet GC Input | GC Output | JP_BUTTON_* |
|--------------------|-----------|-------------|
| A | A | B2 |
| B | B | B1 |
| X | X | B4 |
| Y | Y | B3 |
| Z | Z | R1 |
| L | L | L2 |
| R | R | R2 |
| Start | Start | S2 |
| D-pad | D-pad | DU/DD/DL/DR |
| Menu | Start | A1 |
| Main Stick | Main Stick | LX/LY |
| C-Stick | C-Stick | RX/RY |
| L Trigger | L Trigger | ANALOG_L2 |
| R Trigger | R Trigger | ANALOG_R2 |

LodgeNet-specific encoded buttons (Reset, Star, Order, Hash, Select) are disabled since the GameCube has no corresponding buttons.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| Pico | `make lodgenet2gc_pico` |

## Build and Flash

```bash
make lodgenet2gc_pico
make flash-lodgenet2gc_pico
```

## Wiring

### Pico Pinout

| Pico Pin | GPIO | Function |
|----------|------|----------|
| 4 | GPIO 2 | LodgeNet DATA (input from controller) |
| 5 | GPIO 3 | LodgeNet CLK (output to controller) |
| 6 | GPIO 4 | LodgeNet VCC (powers controller) |
| 7 | GPIO 5 | LodgeNet CLK2 (SNES SR protocol) |
| 10 | GPIO 7 | GC joybus data (bidirectional) |
| 40 | VBUS | 5V power from GC console |
| 38 | GND | Ground |

Connect the RJ11 LodgeNet connector to GPIOs 2-5 and the GameCube controller port data line to GPIO 7. Power the Pico from the GameCube console's 5V supply via VBUS. Same LodgeNet pinout as lodgenet2usb and lodgenet2n64.
