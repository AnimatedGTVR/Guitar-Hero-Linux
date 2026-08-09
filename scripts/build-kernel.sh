#!/bin/bash
set -euo pipefail

# Build the GHL kernel from kernel.org sources.
#
#   KERNEL_VER   kernel version to build (default: pinned below)
#   BUILD_DIR    where sources/outputs go (default: ./build)
#
# The config starts from `make tinyconfig` (everything off) and gets GHL's
# fragment from kernel/config layered on top, resolved with olddefconfig.

KERNEL_VER="${KERNEL_VER:-6.12.102}"
MAJOR="${KERNEL_VER%%.*}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DL_DIR="$BUILD_DIR/dl"
SRC_DIR="$BUILD_DIR/linux-$KERNEL_VER"
OUT_DIR="$BUILD_DIR/out"
GHL_CONFIG="$ROOT_DIR/kernel/config"

TARBALL="linux-$KERNEL_VER.tar.xz"
URL="${MIRROR:-https://cdn.kernel.org/pub/linux/kernel}/v${MAJOR}.x/$TARBALL"

mkdir -p "$DL_DIR" "$OUT_DIR"

if [ ! -e "$DL_DIR/$TARBALL" ]; then
    echo "==> downloading $URL"
    curl -L -o "$DL_DIR/$TARBALL" "$URL"
else
    echo "==> tarball already downloaded ($TARBALL)"
fi

if [ ! -d "$SRC_DIR" ]; then
    echo "==> extracting"
    tar -C "$BUILD_DIR" -xJf "$DL_DIR/$TARBALL"
fi

cd "$SRC_DIR"

echo "==> configuring (tinyconfig + GHL fragment)"
make tinyconfig
cat "$GHL_CONFIG" >> .config
make olddefconfig

echo "==> building bzImage"
make -j"$(nproc)" bzImage

cp arch/x86/boot/bzImage "$OUT_DIR/bzImage"
echo "==> done: $OUT_DIR/bzImage"
