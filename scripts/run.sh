#!/bin/bash
set -euo pipefail

# Boot the latest GHL build in QEMU. Uses KVM when it's available,
# falls back to TCG emulation otherwise.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"

[ -e "$OUT/bzImage" ] || { echo "no bzImage yet — run scripts/build-kernel.sh"; exit 1; }
[ -e "$OUT/initramfs.cpio.gz" ] || { echo "no initramfs yet — run scripts/build-initramfs.sh"; exit 1; }

ACCEL="tcg"
if [ -e /dev/kvm ]; then
    ACCEL="kvm"
fi

exec qemu-system-x86_64 \
    -machine q35 \
    -accel "$ACCEL" \
    -cpu max \
    -m 256 \
    -kernel "$OUT/bzImage" \
    -initrd "$OUT/initramfs.cpio.gz" \
    -append "console=ttyS0 rdinit=/init" \
    -nographic
