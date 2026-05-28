# lodgenet2n64

LodgeNet hotel gaming controller to N64 console.

## Overview

Reads Nintendo LodgeNet hotel controllers via a proprietary 3-wire serial protocol and outputs to an N64 console via the joybus protocol. A cross-console bridge -- no USB involved on either side. Automatically detects N64, GameCube, and SNES LodgeNet controller variants with hot-swap support. All controller types are mapped to N64 output via a passthrough profile.

## Input

[LodgeNet Input](../input/lodgenet.md) -- PIO-based 3-wire serial protocol. MCU protocol for N64/GC (~60Hz), SR protocol for SNES (~131Hz). Auto-detection cycles between protocols.

## Output

[N64 Output](../output/n64.md) -- Joybus PIO protocol on GPIO 7.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | MERGE |
| Player slots | 1 (fixed) |
| LodgeNet Clock pin | GPIO 3 |
| LodgeNet Data pin | GPIO 2 |
| LodgeNet VCC pin | GPIO 4 |
| LodgeNet Clock2 pin | GPIO 5 (SNES SR protocol) |
| N64 Data pin | GPIO 7 |
| Connector (input) | RJ11 6P6C |
| Connector (output) | N64 controller port |

## Key Features

- **Auto-detection** -- N64, GameCube, and SNES LodgeNet controllers detected automatically.
- **Hot-swap** -- Switch between controller types without rebooting.
- **Full analog** -- N64 stick and GC sticks mapped to N64 control stick.
- **C-button mapping** -- N64 C-buttons and GC C-stick both map to N64 C-buttons.
- **Profile system** -- Default passthrough profile (LodgeNet N64 buttons map 1:1 to N64 output).
- **LED indicator** -- Fast blink: N64 console not communicating. Slow blink: no LodgeNet controller. Solid: connected and active.
- **Bootloader reboot** -- Send 'B' via serial to reboot into UF2 bootloader.

## PIO Allocation

| PIO Block | Usage |
|-----------|-------|
| PIO0 | N64 joybus device (Core 1, timing-critical) |
| PIO1 | LodgeNet host (MCU/SR programs, swapped at runtime) |

## Button Mapping

The default profile maps LodgeNet N64 input directly to N64 output:

| LodgeNet Input | Input JP_BUTTON | N64 Output | Profile Remap? |
|----------------|-----------------|------------|----------------|
| A | B1 | A | Direct |
| B | B3 | B | Direct |
| C-Down | B2 | C-Down | Direct |
| C-Left | B4 | C-Left | Direct |
| C-Up | L3 | C-Up | L3 -> R2 |
| C-Right | R3 | C-Right | Direct |
| Z | R1 | Z | R1 -> L2 |
| L | L2 | L | L2 -> L1 |
| R | R2 | R | R2 -> R1 |
| Start | S2 | Start | Direct |
| D-pad | DU/DD/DL/DR | D-pad | Direct |
| Menu | A1 | Start | A1 -> S2 |

The LodgeNet input driver maps Z/L/R and C-Up to different JP_BUTTON positions than the N64 output expects, so the profile remaps them. LodgeNet-specific encoded buttons (Reset, Star, Order, Hash, Select) are disabled since the N64 has no corresponding buttons.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| Pico | `make lodgenet2n64_pico` |

## Build and Flash

```bash
make lodgenet2n64_pico
make flash-lodgenet2n64_pico
```

## Wiring

### Pico Pinout

| Pico Pin | GPIO | Function |
|----------|------|----------|
| 4 | GPIO 2 | LodgeNet DATA (input from controller) |
| 5 | GPIO 3 | LodgeNet CLK (output to controller) |
| 6 | GPIO 4 | LodgeNet VCC (powers controller) |
| 7 | GPIO 5 | LodgeNet CLK2 (SNES SR protocol) |
| 10 | GPIO 7 | N64 joybus data (bidirectional) |
| 40 | VBUS | 5V power from N64 console |
| 38 | GND | Ground |

Connect the RJ11 LodgeNet connector to GPIOs 2-5 and the N64 controller port data line to GPIO 7. Power the Pico from the N64 console's 5V supply via VBUS.
