#!/usr/bin/env python3
"""
usbgba-multiboot.py — drive a real Kawasedo multiboot through the
USB vendor bridge, no Dolphin. Verifies the bridge+joybus path can
sustain a full multiboot byte sequence end-to-end.

Mirrors the protocol that eth-multiboot.js drives on the TCP path,
but talks libusb to the firmware's USB_OUTPUT_MODE_GBA_LINK vendor
endpoint instead. If this succeeds, the bridge architecture is
solid and any remaining failure with real Dolphin is in
Dolphin-side timing / Madden-specific.

Default ROM: /tmp/joypad_mb.gba (the same payload eth-multiboot.js
uses on the TCP path).

Usage:
    python3.13 tools/gba-bridge/usbgba-multiboot.py [--rom PATH] [--vid 0x057E] [--pid 0x0338]
"""

import argparse
import struct
import sys
import time

try:
    import usb.core
    import usb.util
except ImportError:
    print("ERROR: pyusb not installed. Run: pip install pyusb", file=sys.stderr)
    sys.exit(1)


# Kawasedo cipher constants (mirrors tools/gba-bridge/eth-multiboot.js)
MAGIC_SEDO = 0x6F646573  # "sedo"
MAGIC_KAWA = 0x6177614B  # "Kawa"
MAGIC_BY   = 0x42424242  # "BBBB"

# Joybus command bytes
CMD_RESET  = 0xFF
CMD_STATUS = 0x00
CMD_READ   = 0x14
CMD_WRITE  = 0x15

# Joybus status bits
JSTAT_PSF0 = 0x10
JSTAT_RECV = 0x08


def open_bridge(vid: int, pid: int):
    dev = usb.core.find(idVendor=vid, idProduct=pid)
    if dev is None:
        print(f"ERROR: no bridge at VID 0x{vid:04X} PID 0x{pid:04X}", file=sys.stderr)
        sys.exit(2)
    cfg = dev.get_active_configuration()
    iface = next((i for i in cfg if i.bInterfaceClass == 0xFF), None)
    if iface is None:
        print("ERROR: no vendor iface", file=sys.stderr)
        sys.exit(3)
    try:
        if dev.is_kernel_driver_active(iface.bInterfaceNumber):
            dev.detach_kernel_driver(iface.bInterfaceNumber)
    except (NotImplementedError, usb.core.USBError):
        pass
    usb.util.claim_interface(dev, iface.bInterfaceNumber)
    out_ep = next(ep for ep in iface
                  if usb.util.endpoint_direction(ep.bEndpointAddress)
                  == usb.util.ENDPOINT_OUT)
    in_ep = next(ep for ep in iface
                 if usb.util.endpoint_direction(ep.bEndpointAddress)
                 == usb.util.ENDPOINT_IN)
    return dev, iface, out_ep, in_ep


def xfer(out_ep, in_ep, tx: bytes, rx_len: int, timeout_ms: int = 200) -> bytes:
    out_ep.write(tx, timeout=timeout_ms)
    return bytes(in_ep.read(rx_len, timeout=timeout_ms))


def reset(out_ep, in_ep):
    return xfer(out_ep, in_ep, bytes([CMD_RESET]), 3)


def status(out_ep, in_ep):
    return xfer(out_ep, in_ep, bytes([CMD_STATUS]), 3)


def read_session_key(out_ep, in_ep):
    return xfer(out_ep, in_ep, bytes([CMD_READ]), 5)


def write_word(out_ep, in_ep, word: int):
    return xfer(out_ep, in_ep,
                bytes([CMD_WRITE,
                       word & 0xFF, (word >> 8) & 0xFF,
                       (word >> 16) & 0xFF, (word >> 24) & 0xFF]),
                1)


def calc_gc_key(rom_len: int) -> int:
    # Same derivation eth-multiboot.js uses.
    bytes_count = (rom_len - 0x200) >> 3
    res = 0x380000
    res |= (bytes_count & 0x7F)
    res |= ((bytes_count >> 7) & 0x7F) << 8
    res |= ((bytes_count >> 14) & 1) << 16
    return ((res | 0x80808080) ^ MAGIC_KAWA) & 0xFFFFFFFF


def multiboot(out_ep, in_ep, rom: bytes):
    rom_len = len(rom)
    # Pad to multiple of 16 bytes
    pad = (-rom_len) & 0xF
    if pad:
        rom = rom + bytes(pad)
        rom_len = len(rom)
    print(f"[mb] starting multiboot of {rom_len} bytes (0x{rom_len:X})")

    # Step 1: RESET
    print("[mb] step 1: RESET")
    rx = reset(out_ep, in_ep)
    print(f"[mb]   rx {rx.hex()}")
    if rx[0] != 0x00 or rx[1] != 0x04:
        print(f"[mb] FAIL: expected type 0x0400, got 0x{rx[1]:02X}{rx[0]:02X}")
        return False

    # Step 2: STATUS poll for PSF0
    print("[mb] step 2: STATUS (poll for PSF0)")
    for i in range(100):
        rx = status(out_ep, in_ep)
        if rx[2] & JSTAT_PSF0:
            print(f"[mb]   PSF0 set on poll {i}: rx={rx.hex()}")
            break
    else:
        print("[mb] FAIL: PSF0 never set")
        return False

    # Step 3: READ session key
    print("[mb] step 3: READ session_key")
    for i in range(20):
        rx = read_session_key(out_ep, in_ep)
        seed = struct.unpack("<I", rx[:4])[0]
        if seed != 0:
            print(f"[mb]   seed=0x{seed:08X} jstat=0x{rx[4]:02X} (poll {i})")
            break
    else:
        print("[mb] FAIL: session key never returned")
        return False

    session_key = seed ^ MAGIC_SEDO

    # Step 4: WRITE our_key
    our_key = calc_gc_key(rom_len)
    print(f"[mb] step 4: WRITE our_key=0x{our_key:08X}")
    write_word(out_ep, in_ep, our_key)

    # Step 5: streaming header (192 bytes plaintext)
    print("[mb] step 5: streaming header (192 bytes, plaintext)")
    for off in range(0, 0xC0, 4):
        word = struct.unpack("<I", rom[off:off+4])[0]
        write_word(out_ep, in_ep, word)

    # Step 6: streaming body (encrypted)
    print(f"[mb] step 6: streaming body ({rom_len - 0xC0} bytes, encrypted)")
    t0 = time.perf_counter()
    f_crc = 0xFFFF_FFFF
    sk = session_key
    last_pct = -1
    for off in range(0xC0, rom_len, 4):
        word = struct.unpack("<I", rom[off:off+4])[0]
        sk = (sk * 0x6F646573 + 1) & 0xFFFFFFFF  # MAGIC_KAWA-pattern; matches firmware
        # encryption: word XOR session_key XOR (~(off + 0x20<<20) + 1) XOR MAGIC_BY
        enc = (word ^ sk ^ ((~(off + (0x20 << 20)) + 1) & 0xFFFFFFFF) ^ MAGIC_BY) & 0xFFFFFFFF
        write_word(out_ep, in_ep, enc)
        # CRC accumulator (Kawasedo polynomial)
        for b in [enc & 0xFF, (enc >> 8) & 0xFF, (enc >> 16) & 0xFF, (enc >> 24) & 0xFF]:
            f_crc ^= b
            for _ in range(8):
                f_crc = ((f_crc >> 1) ^ 0xC37B) if (f_crc & 1) else (f_crc >> 1)
        # Coarse progress every 25%
        pct = (off - 0xC0) * 100 // (rom_len - 0xC0)
        if pct // 25 != last_pct // 25:
            last_pct = pct
            elapsed = time.perf_counter() - t0
            rate = (off - 0xC0) / 1024 / elapsed if elapsed > 0 else 0
            print(f"[mb]   progress: 0x{off:X}/{rom_len:X} ({pct}%) {rate:.2f} KB/s")
    body_time = time.perf_counter() - t0
    print(f"[mb]   body done in {body_time:.2f}s "
          f"({(rom_len-0xC0)/1024/body_time:.2f} KB/s)")

    # Step 7: WRITE final CRC
    print(f"[mb] step 7: WRITE final fcrc=0x{f_crc:04X}")
    write_word(out_ep, in_ep, f_crc)

    # Step 8: READ CRC reply
    print("[mb] step 8: READ crc_reply")
    rx = read_session_key(out_ep, in_ep)
    print(f"[mb]   crc_reply: {rx.hex()}")

    print("[mb] ✓ multiboot complete — GBA should be running the uploaded ROM")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", default="/tmp/joypad_mb.gba")
    ap.add_argument("--vid", type=lambda s: int(s, 0), default=0x057E)
    ap.add_argument("--pid", type=lambda s: int(s, 0), default=0x0338)
    args = ap.parse_args()

    with open(args.rom, "rb") as f:
        rom = f.read()
    print(f"[main] loaded ROM {args.rom} ({len(rom)} bytes)")

    dev, iface, out_ep, in_ep = open_bridge(args.vid, args.pid)
    try:
        ok = multiboot(out_ep, in_ep, rom)
        sys.exit(0 if ok else 1)
    finally:
        usb.util.release_interface(dev, iface.bInterfaceNumber)


if __name__ == "__main__":
    main()
