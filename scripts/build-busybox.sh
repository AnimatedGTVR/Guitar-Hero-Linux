#!/bin/bash
set -euo pipefail

# Build busybox for GHL — the base userspace (ash shell + coreutils) for the
# initramfs. Static, so it needs nothing else at boot.
#
# BUSYBOX_VER  version to build (default: pinned below)

BUSYBOX_VER="${BUSYBOX_VER:-1.38.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DL_DIR="$BUILD_DIR/dl"
SRC_DIR="$BUILD_DIR/busybox-$BUSYBOX_VER"
OUT_DIR="$BUILD_DIR/out"

TARBALL="busybox-$BUSYBOX_VER.tar.bz2"
URL="https://busybox.net/downloads/$TARBALL"

mkdir -p "$DL_DIR" "$OUT_DIR"

if [ ! -e "$DL_DIR/$TARBALL" ]; then
    echo "==> downloading $URL"
    curl -L -o "$DL_DIR/$TARBALL" "$URL"
else
    echo "==> tarball already downloaded ($TARBALL)"
fi

if [ ! -d "$SRC_DIR" ]; then
    echo "==> extracting"
    tar -C "$BUILD_DIR" -xjf "$DL_DIR/$TARBALL"
fi

cd "$SRC_DIR"

echo "==> configuring (static)"
make defconfig >/dev/null
sed -i 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/' .config
# tc won't compile against kernel >= 7 headers and the guitar has no
# traffic control needs anyway.
sed -i 's/^CONFIG_TC=y$/# CONFIG_TC is not set/' .config
make oldconfig >/dev/null

echo "==> building"
make -j"$(nproc)" busybox

cp busybox "$OUT_DIR/busybox"
echo "==> done: $OUT_DIR/busybox"
