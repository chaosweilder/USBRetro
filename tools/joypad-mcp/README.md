# joypad-mcp

MCP server that lets Claude Code (or any MCP client — Codex, Cursor, etc.) drive a Joypad OS adapter as a synthetic player and see the result via screenshots.

The plan is in [`.dev/docs/joypad-mcp.md`](../../.dev/docs/joypad-mcp.md). This README is just install + smoke test.

## What it does

- Sends `UART_PKT_AI_INJECT` frames over the same UART/USB-CDC port we already use for adapter logs — no firmware changes needed for any app that already calls `uart_host_init()`.
- Exposes 19 MCP tools: `list_adapters`, `connect`, `disconnect`, `info`, `tap`, `press`, `hold`, `release`, `axis`, `axes`, `combo`, `release_all`, `set_blend_mode`, `read_state`, `read_human_input`, `read_log`, `set_capture_source`, `screenshot`, `screenshot_diff`.
- Returns desktop screenshots inline (Phase 1, macOS only). HDMI capture cards land in Phase 2.

## Install

```sh
cd tools/joypad-mcp
npm install
npm run build
npm test           # 9 tests, all unit (no hardware needed)
```

`npm run build` writes ES modules to `dist/`. The entrypoint is `dist/server.js`, registered as the `joypad-mcp` bin.

## Wire it into Claude Code

Add to `~/.claude.json` (or this project's `.mcp.json`):

```json
{
  "mcpServers": {
    "joypad": {
      "command": "node",
      "args": ["/Users/you/git/joypad/joypad-os/tools/joypad-mcp/dist/server.js"]
    }
  }
}
```

Restart Claude Code. The 19 tools should appear. `claude mcp` lists them.

## Smoke test against real hardware

Phase 1 target: a `usb2usb_kb2040` build acting as a USB HID gamepad on your laptop.

1. Flash a KB2040 with `make usb2usb_kb2040` and a build of any app that calls `uart_host_init()`. (Most do — check the app's `app.c`.)
2. Plug it into your laptop. Note the serial port — usually `/dev/cu.usbmodem*` on macOS.
3. Open Claude Code in this repo and ask: *"Connect to the joypad adapter and tap A."* The assistant should call `list_adapters` → `connect` → `tap`. You'll see a "button A" event in any HID test app on the laptop.

If `tap` doesn't register: call `read_log` to see if `[uart_host]` printed anything. The most common cause is the firmware app not calling `uart_host_init()` — fix at the app side.

## Buttons & axes

W3C-canonical names (matches `src/core/buttons.h`):

```
B1 B2 B3 B4         face buttons (A/B/X/Y in XInput, B/A/Y/X in Switch)
DU DD DL DR         d-pad
L1 R1 L2 R2         shoulders / triggers
S1 S2               select / start
L3 R3               stick clicks
A1                  guide / home / PS

Aliases: UP=DU DOWN=DD LEFT=DL RIGHT=DR START=S2 SELECT=S1 A=B1 B=B2 X=B3 Y=B4 ...

Axes: LX LY RX RY (0-255, 128=center)  LT RT (0-255, 0=released)
Combine buttons with +: "B1+DR"
```

## Architecture

```
src/
├── server.ts          # stdio MCP entry, registers tools
├── transport.ts       # SerialPort + CRC8 + 0xAA frame demux from log stream
├── protocol.ts        # packet types + struct encoders mirroring uart_protocol.h
├── buttons.ts         # name <-> JP_BUTTON_* bit mask
├── state.ts           # connection + per-slot held buttons / analog
└── tools/
    ├── connection.ts  # list_adapters, connect, disconnect, info
    ├── input.ts       # tap, press, hold, release, axis, axes, combo, release_all, set_blend_mode
    ├── observe.ts     # read_state, read_human_input, read_log
    └── vision.ts      # set_capture_source, screenshot, screenshot_diff
```

The firmware path is the existing `UART_PKT_AI_INJECT (0x50)` parsed at `src/native/host/uart/uart_host.c:132`. Same wire as `.dev/docs/AI_CONTROLLER_POC.md`'s autonomous Python loop — different driver shape (interactive vs headless).

## Limits

- Round-trip is ~3-6s per assistant decision (cloud LLM dominates). Playable: turn-based games, menus, slow puzzles. Not playable: fighting games, action platformers.
- One adapter per process. Multiple adapters = run multiple MCP servers, each with its own `joypad-N` name in the config.
- Phase 1 is macOS desktop screenshot only. Linux + UVC capture cards are Phase 2.
- Right now the firmware doesn't echo state back over UART, so `read_state` reflects what the MCP last *sent*, not what the console actually applied. `read_human_input` is forward-looking — it'll work as soon as any app starts emitting `INPUT_EVENT` packets back.
