#!/bin/bash
set -euo pipefail

# Build a bootable GHL ISO with GRUB. grub-mkrescue produces a hybrid
# BIOS + UEFI image that boots on real hardware or in QEMU.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="$BUILD_DIR/out"
ISO_DIR="$BUILD_DIR/iso"

[ -e "$OUT_DIR/bzImage" ] || { echo "no bzImage yet — run scripts/build-kernel.sh"; exit 1; }
[ -e "$OUT_DIR/initramfs.cpio.gz" ] || { echo "no initramfs yet — run scripts/build-initramfs.sh"; exit 1; }

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"

cp "$OUT_DIR/bzImage" "$ISO_DIR/boot/"
cp "$OUT_DIR/initramfs.cpio.gz" "$ISO_DIR/boot/"
cp "$SCRIPT_DIR/iso/grub.cfg" "$ISO_DIR/boot/grub/"

echo "==> assembling ISO"
grub-mkrescue -o "$OUT_DIR/ghl.iso" "$ISO_DIR"

echo "==> done: $OUT_DIR/ghl.iso"
