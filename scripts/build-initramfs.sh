#!/bin/bash
set -euo pipefail

# Build the GHL initramfs: a static /init plus the empty mount points it
# creates at boot. Everything else is empty until packages exist.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="$BUILD_DIR/out"
STAGE="$BUILD_DIR/initramfs"

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/dev" "$STAGE/proc" "$STAGE/sys" "$STAGE/tmp" "$OUT_DIR"

echo "==> building init (static)"
gcc -static -O2 -o "$STAGE/init" "$ROOT_DIR/init/init.c"

echo "==> packing initramfs"
cd "$STAGE"
find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 > "$OUT_DIR/initramfs.cpio.gz"

echo "==> done: $OUT_DIR/initramfs.cpio.gz"
