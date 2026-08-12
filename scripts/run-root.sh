#!/bin/bash
set -euo pipefail

# Boot the GHL kernel + thin initramfs + real rootfs (build/out/rootfs.ext4)
# in QEMU. init pivots out of the initramfs into the on-disk system.
#
# A window opens directly into the framebuffer root terminal. Run `installer`
# for system setup or `ampkg` for packages. Pass --console to run QEMU entirely
# in this terminal with an interactive serial root shell. Power off with
# `poweroff`; in console mode Ctrl-A X immediately exits QEMU.
#
#   QEMU_DISPLAY  display backend (default: "gtk,gl=on,zoom-to-fit=on")
#   QEMU_MEMORY   guest RAM (default: 6144M — see note below)
#   QEMU_CPUS     virtual CPU count (default: 4)
#   QEMU_AUDIO    audio backend (default: pipewire)
#   QEMU_RESOLUTION framebuffer size (default: 1280x720)
#
# Clone Hero's Unity player sizes a TLSF memory pool off total system RAM,
# and TLSF's own pool-size check is an exclusive < 4GiB bound — exactly
# 4096M of guest RAM trips "tlsf_add_pool: Memory size must be between
# 0x28 and 0x100000000 bytes" and segfaults right after. Default past that
# edge.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
DISPLAY_BACKEND="${QEMU_DISPLAY:-gtk,gl=on,zoom-to-fit=on}"
MEMORY="${QEMU_MEMORY:-6144M}"
CPUS="${QEMU_CPUS:-4}"
AUDIO_BACKEND="${QEMU_AUDIO:-pipewire}"
RESOLUTION="${QEMU_RESOLUTION:-1280x720}"
CONSOLE_MODE=0

case "$RESOLUTION" in
    *x*)
        VIDEO_WIDTH=${RESOLUTION%x*}
        VIDEO_HEIGHT=${RESOLUTION#*x}
        ;;
    *)
        echo "invalid QEMU_RESOLUTION: $RESOLUTION (expected WIDTHxHEIGHT)" >&2
        exit 2
        ;;
esac
case "$VIDEO_WIDTH" in ''|*[!0-9]*) echo "invalid display width: $VIDEO_WIDTH" >&2; exit 2 ;; esac
case "$VIDEO_HEIGHT" in ''|*[!0-9]*) echo "invalid display height: $VIDEO_HEIGHT" >&2; exit 2 ;; esac
if [ "$VIDEO_WIDTH" -lt 640 ] || [ "$VIDEO_WIDTH" -gt 7680 ] ||
   [ "$VIDEO_HEIGHT" -lt 480 ] || [ "$VIDEO_HEIGHT" -gt 4320 ]; then
    echo "unsupported QEMU resolution: $RESOLUTION" >&2
    exit 2
fi

case "${1:-}" in
    --console|-c)
        CONSOLE_MODE=1
        ;;
    "")
        ;;
    *)
        echo "usage: $0 [--console]" >&2
        exit 2
        ;;
esac

[ -e "$OUT/bzImage" ] || { echo "no bzImage yet — run scripts/build-kernel.sh"; exit 1; }
[ -e "$OUT/initramfs.cpio.gz" ] || { echo "no initramfs yet — run scripts/build-initramfs.sh"; exit 1; }
[ -e "$OUT/rootfs.ext4" ] || { echo "no rootfs yet — run scripts/build-rootfs.sh"; exit 1; }

ACCEL="tcg"
CPU="max"
if [ -e /dev/kvm ]; then
    ACCEL="kvm"
    CPU="host"
fi

QEMU_ARGS=(
    -machine q35 \
    -accel "$ACCEL" \
    -cpu "$CPU" \
    -smp "$CPUS" \
    -m "$MEMORY" \
    -kernel "$OUT/bzImage" \
    -initrd "$OUT/initramfs.cpio.gz" \
    -drive file="$OUT/rootfs.ext4",format=raw,if=virtio \
    -netdev user,id=n0 \
    -device virtio-net-pci,netdev=n0 \
    -device virtio-keyboard-pci \
    -device virtio-mouse-pci \
    -append "console=tty1 console=ttyS0 rdinit=/init root=/dev/vda video=Virtual-1:${VIDEO_WIDTH}x${VIDEO_HEIGHT}@60 fbcon=font:VGA8x16 consoleblank=0 quiet loglevel=3"
)

if [ "$CONSOLE_MODE" -eq 1 ]; then
    echo "==> booting GHL in this terminal (Ctrl-A X exits QEMU)"
    QEMU_ARGS+=(
        -device virtio-vga
        -display none
        -serial mon:stdio
    )
else
    echo "==> booting GHL ($RESOLUTION, display: $DISPLAY_BACKEND, serial log: $OUT/ghl-serial.log)"
    QEMU_ARGS+=(
        -device virtio-vga-gl,hostmem=4G,blob=true,venus=true,xres="$VIDEO_WIDTH",yres="$VIDEO_HEIGHT"
        -audiodev driver="$AUDIO_BACKEND",id=audio0
        -device intel-hda
        -device hda-duplex,audiodev=audio0
        -display "$DISPLAY_BACKEND"
        -serial file:"$OUT/ghl-serial.log"
    )
fi

exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
