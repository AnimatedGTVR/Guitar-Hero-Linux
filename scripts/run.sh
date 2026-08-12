#!/bin/bash
set -euo pipefail

# Boot the latest GHL build (initramfs only) in QEMU. Uses KVM when it's
# available, falls back to TCG emulation otherwise. A window opens with the
# framebuffer console; the serial console is saved to build/out/ghl-serial.log.
#
#   QEMU_DISPLAY  display backend (default: "default"; use "none" headless)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
DISPLAY_BACKEND="${QEMU_DISPLAY:-default}"

[ -e "$OUT/bzImage" ] || { echo "no bzImage yet — run scripts/build-kernel.sh"; exit 1; }
[ -e "$OUT/initramfs.cpio.gz" ] || { echo "no initramfs yet — run scripts/build-initramfs.sh"; exit 1; }

ACCEL="tcg"
if [ -e /dev/kvm ]; then
    ACCEL="kvm"
fi

echo "==> booting GHL (display: $DISPLAY_BACKEND, serial log: $OUT/ghl-serial.log)"
exec qemu-system-x86_64 \
    -machine q35 \
    -accel "$ACCEL" \
    -cpu max \
    -m 256 \
    -kernel "$OUT/bzImage" \
    -initrd "$OUT/initramfs.cpio.gz" \
    -netdev user,id=n0 \
    -device virtio-net-pci,netdev=n0 \
    -device virtio-vga \
    -device virtio-keyboard-pci \
    -device virtio-mouse-pci \
    -display "$DISPLAY_BACKEND" \
    -serial file:"$OUT/ghl-serial.log" \
    -append "console=tty1 console=ttyS0 rdinit=/init"
