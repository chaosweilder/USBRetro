# Wii Extension Input Interface

Reads Wii extension accessories (Nunchuck, Classic Controller, Classic Pro, Guitar Hero, Drums, DJ Hero Turntable, Taiko drum, uDraw tablet, MotionPlus) over I²C at 400 kHz-tolerant 50 kHz. A cut Wii extension cable, an Adafruit Nunchuck Qwiic breakout, or any extension-to-Qwiic adapter plugs straight into the microcontroller's I²C pins and the accessory is auto-detected on plug-in.

## Protocol

- **Bus**: I²C fast-mode (50 kHz by default, tolerant up to 400 kHz)
- **Address**: 0x52 (every Wii extension uses this same 7-bit address)
- **Transport**: RP2040 hardware I²C via `platform_i2c.h` HAL
- **Polling**: 500 Hz when connected, 4 Hz retry while scanning
- **Location**: `src/native/host/wii/` (host adapter), `src/lib/wii_ext/` (portable protocol library)

The bus protocol is Nintendo's standard unencrypted init sequence:

1. Write `0xF0 = 0x55` to initialize without encryption
2. Write `0xFB = 0x00` to finalize init
3. Read 6 bytes from `0xFA` to get the extension ID
4. Optionally read 32 bytes of calibration from `0x20` with a `0x55`-seeded sum checksum
5. Poll for state by reading 6-8 bytes from `0x00`

Clone accessories sometimes NACK the first init write; the driver retries up to 10 times with 20 ms between attempts. Any I²C error during polling marks the extension disconnected and re-runs init on the next tick — no DETECT pin required.

## Supported Accessories

Auto-detected via the ID bytes at `0xFA..0xFF`:

| Accessory | ID selector | `wii_ext_type_t` | `controller_layout_t` |
|-----------|-------------|------------------|------------------------|
| Nunchuck | `id[5]=0x00` | `NUNCHUCK` | `LAYOUT_WII_NUNCHUCK` |
| Classic Controller | `id[0]=0x00, id[5]=0x01` | `CLASSIC` | `LAYOUT_WII_CLASSIC` |
| Classic Controller Pro | `id[0]=0x01, id[5]=0x01` | `CLASSIC_PRO` | `LAYOUT_WII_CLASSIC_PRO` |
| Guitar Hero 3 Guitar | `id[0]=0x00, id[5]=0x03` | `GUITAR` | `LAYOUT_WII_GUITAR` |
| Guitar Hero World Tour Guitar | `id[0]=0x01, id[5]=0x03` | `GUITAR` | `LAYOUT_WII_GUITAR` |
| Rock Band / GH Drums | `id[0]=0x01, id[5]=0x03` | `GUITAR` ¹ | `LAYOUT_WII_GUITAR` ¹ |
| DJ Hero Turntable | `id[0]=0x03, id[5]=0x03` | `TURNTABLE` | `LAYOUT_WII_TURNTABLE` |
| MotionPlus (standalone) | `id[5]=0x05` | `MOTIONPLUS` | `LAYOUT_WII_MOTIONPLUS` |
| Taiko no Tatsujin Drum | `id[5]=0x11` | `TAIKO` | `LAYOUT_WII_TAIKO` |
| THQ uDraw Tablet | `id[5]=0x12` or `0x13` | `UDRAW` | `LAYOUT_WII_UDRAW` |

¹ Drums and GHWT Guitar share the same ID bytes. See "Known issues" below.

## Button Mapping (Nunchuck)

| Nunchuck | JP_BUTTON_* | Notes |
|----------|-------------|-------|
| C | B1 | Primary |
| Z | B2 | Secondary |
| Stick | LX / LY | Y inverted to HID convention |
| Accelerometer | `event.accel[]` | 3-axis, ±2 g range, flows through to SInput motion |

## Button Mapping (Classic / Classic Pro)

Physical layout → W3C face positions:

| Wii button | Position | JP_BUTTON_* |
|------------|----------|-------------|
| B | South | B1 |
| A | East  | B2 |
| Y | West  | B3 |
| X | North | B4 |
| L | shoulder | L1 |
| R | shoulder | R1 |
| ZL | trigger | L2 |
| ZR | trigger | R2 |
| − (minus) | select | S1 |
| + (plus) | start | S2 |
| Home | home | A1 |
| D-pad | D-pad | DU/DD/DL/DR |

Analog:
- Left stick → LX/LY (6-bit upscaled)
- Right stick → RX/RY (5-bit upscaled)
- Classic analog L/R triggers → L2/R2 analog axes
- Classic Pro has digital-only triggers (analog axes read 0)

## Button Mapping (Guitar Hero 3 / World Tour)

| Guitar button | JP_BUTTON_* |
|---------------|-------------|
| Green fret | B1 |
| Red fret | B2 |
| Yellow fret | B3 |
| Blue fret | B4 |
| Orange fret | L1 |
| Strum Up | DU |
| Strum Down | DD |
| − (minus) | S1 |
| + (plus) | S2 |
| Whammy bar | R2 (analog) |

GHWT's touch-fret slider (capacitive strip) is quantized to the five fret zones and OR'd into the same fret buttons — touching the green zone fires Green, sliding up toward Red fires both, etc.

## Button Mapping (Drums)

Each drum pad is a digital button with optional 4-bit velocity surfaced on the PS3 pressure channel:

| Drum pad | JP_BUTTON_* | `event.pressure[]` slot |
|----------|-------------|-------------------------|
| Red | B1 | pressure[10] (cross) |
| Yellow | B3 | pressure[8] (triangle) |
| Blue | B4 | pressure[9] (circle) |
| Green | B2 | pressure[11] (square) |
| Orange cymbal | L1 | pressure[6] |
| Bass pedal | R1 | pressure[7] |
| − / + | S1 / S2 | — |

Velocity is 0..255 (scaled from the 4-bit native range) and flows through to PS3 output mode for rhythm games that read pressure.

## Button Mapping (DJ Hero Turntable)

Turntable rotations are mapped to analog sticks with accumulator + spring-to-center:

| Control | Maps to |
|---------|---------|
| Left turntable rotation | LX (accumulated around 512 center) |
| Right turntable rotation | RX (accumulated) |
| Crossfader | L2 (4-bit → 10-bit) |
| Effects dial | R2 (5-bit → 10-bit) |
| Left Green / Red / Blue | B1 / B2 / B3 |
| Right Green / Red / Blue | B4 / L1 / R1 |
| Euphoria | A1 |
| − / + | S1 / S2 |

## Button Mapping (Taiko Drum)

The TaTaCon has four velocity-less strike sensors:

| Surface | JP_BUTTON_* |
|---------|-------------|
| Left face (don) | B1 |
| Right face (don) | B2 |
| Left rim (ka) | B3 |
| Right rim (ka) | B4 |
| − / + | S1 / S2 |

## Button Mapping (uDraw Tablet)

| uDraw | Where |
|-------|-------|
| Pen X / Y | `event.touch[0].x/y`, and scaled into LX/LY |
| Pen pressure | `event.touch[0]` pressure, and L2 analog |
| Pen button 1 | B1 |
| Pen button 2 | B2 |

## MotionPlus

Standalone MotionPlus surfaces 3-axis gyro in `event.gyro[]` with an effective ±2000 dps range (slow-mode readings are normalised to fast-mode scale). The existing SInput HID descriptor already carries gyro, so motion flows end-to-end with no descriptor changes.

MotionPlus **passthrough** mode — where MotionPlus chains a Nunchuck or Classic behind it and produces interleaved reports — is not yet implemented. See the CAVEAT block in `src/lib/wii_ext/ext_motionplus.c` for the activation sequence and work required to add it.

## Known Issues

- **Drums / GHWT Guitar ID collision**: both report `id[0]=0x01, id[5]=0x03`. Current dispatch defaults to Guitar. Disambiguating requires sniffing report structure (drums emit a pad-id + velocity pattern in byte 2 that guitars don't). See the CAVEAT in `src/lib/wii_ext/ext_drums.c`.
- **DJ Hero Turntable**: bit-packing is extremely dense and the parser is untested. Specific fields likely to need adjustment: sign polarity, crossfader direction, effects-dial wrap. See CAVEAT in `src/lib/wii_ext/ext_turntable.c`.
- **uDraw Tablet**: layout is community-reverse-engineered, not authoritative. X/Y ranges, nibble-pack order, pressure threshold, and button bit positions all need real-hardware tuning. See CAVEAT in `src/lib/wii_ext/ext_udraw.c`.

## Wiring

Four-wire interface:

| Wii extension | MCU pin (default for KB2040 STEMMA QT) |
|---------------|-----------------------------------------|
| 3V3 | 3V3 pad |
| GND | GND pad |
| SDA | GP12 |
| SCL | GP13 |

**Do not power the Wii extension from 5 V.** All accessories are 3.3 V-only parts; 5 V will damage them.

External 2.2–4.7 kΩ pull-ups to 3V3 are required on SDA and SCL. The KB2040's STEMMA QT port includes them. Most breakouts (Adafruit 4836, similar) include them too.

## Apps Using This Interface

- [wii2usb](../apps/wii2usb.md) — Wii extension → USB HID gamepad (SInput / XInput / PS3 / etc.)
- [wii2gc](../apps/wii2gc.md) — Wii extension → GameCube console (joybus)
- [wii2n64](../apps/wii2n64.md) — Wii extension → N64 console (joybus)
