#!/bin/bash
# flash-loop.sh — Flash multiple RP2040 boards in sequence
#
# Watches for the RPI-RP2 bootloader drive and copies the specified UF2 file.
# After each flash, waits for the drive to disappear before watching for the
# next board. Plug in each board in bootloader mode (hold BOOTSEL while
# connecting USB). Press Ctrl+C to stop.
#
# Usage:
#   ./tools/flash-loop.sh <path-to-uf2>
#   ./tools/flash-loop.sh releases/joypad_abc1234_usb2pce_kb2040.uf2

set -e

UF2="$1"
if [ -z "$UF2" ] || [ ! -f "$UF2" ]; then
    echo "Usage: $0 <path-to-uf2>"
    echo "Example: $0 releases/joypad_abc1234_usb2pce_kb2040.uf2"
    exit 1
fi

COUNT=0
DRIVE="/Volumes/RPI-RP2"

echo "=== Joypad Flash Loop ==="
echo "UF2: $UF2"
echo "Plug in each board in bootloader mode. Ctrl+C to stop."
echo ""

while true; do
    if [ -d "$DRIVE" ] && [ -w "$DRIVE" ]; then
        COUNT=$((COUNT + 1))
        echo -n "[$COUNT] Flashing... "
        cp "$UF2" "$DRIVE/" 2>/dev/null &
        CP_PID=$!
        sleep 3
        kill $CP_PID 2>/dev/null
        wait $CP_PID 2>/dev/null
        echo "done!"
        # Wait for drive to fully disappear before looking for next
        while [ -d "$DRIVE" ]; do
            sleep 0.5
        done
        # Extra settle time for macOS USB stack
        sleep 2
        echo "    Ready for next board."
    fi
    sleep 0.5
done
