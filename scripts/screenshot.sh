#!/bin/bash
set -euo pipefail

# Boot GHL with a virtual display (virtio-gpu) and grab a screenshot of the
# framebuffer, so the boot splash / console can be "seen" without a monitor.
#
#   scripts/screenshot.sh [out.ppm]    (default: build/out/ghl-screen.ppm)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
SHOT="${1:-$OUT/ghl-screen.ppm}"

[ -e "$OUT/bzImage" ] || { echo "no bzImage yet — run scripts/build-kernel.sh"; exit 1; }
[ -e "$OUT/initramfs.cpio.gz" ] || { echo "no initramfs yet — run scripts/build-initramfs.sh"; exit 1; }
[ -e "$OUT/rootfs.ext4" ] || { echo "no rootfs yet — run scripts/build-rootfs.sh"; exit 1; }

ACCEL="tcg"
[ -e /dev/kvm ] && ACCEL="kvm"

MON="/tmp/ghl-mon.sock"
rm -f "$MON" "$SHOT"

qemu-system-x86_64 \
    -machine q35 \
    -accel "$ACCEL" \
    -cpu max \
    -m 512 \
    -kernel "$OUT/bzImage" \
    -initrd "$OUT/initramfs.cpio.gz" \
    -drive file="$OUT/rootfs.ext4",format=raw,if=virtio \
    -netdev user,id=n0 \
    -device virtio-net-pci,netdev=n0 \
    -device virtio-vga \
    -audiodev driver=none,id=audio0 \
    -device intel-hda \
    -device hda-duplex,audiodev=audio0 \
    -display vnc=127.0.0.1:0 \
    -monitor unix:"$MON",server=on,wait=off \
    -serial file:/tmp/ghl-screenshot.log \
    -append "console=ttyS0 rdinit=/init root=/dev/vda" &
QPID=$!

# give the splash time to draw, then ask the monitor for the framebuffer
for _ in $(seq 1 30); do
    [ -S "$MON" ] && break
    sleep 0.5
done
sleep 10
printf 'screendump %s\n' "$SHOT" | socat - UNIX-CONNECT:"$MON" >/dev/null 2>&1 || true
sleep 2
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

if [ -s "$SHOT" ]; then
    echo "==> screenshot: $SHOT ($(wc -c < "$SHOT") bytes)"
else
    echo "!! screendump failed (serial log in /tmp/ghl-screenshot.log)" >&2
    exit 1
fi
