#!/bin/bash
set -euo pipefail

# Assemble the GHL real root filesystem with ampkg and pack it into an
# ext4 disk image. Booting this (see run-root.sh) makes init pivot out of
# the initramfs into a real on-disk system.
#
#   ROOTFS_SIZE  sparse image size (default: 4G, for Clone Hero + songs)
#   AMPKG        path to the ampkg binary (default: tools/ampkg/ampkg)
#   INSTALL_BACKSTAGE  include the graphical shell (default: 1; set 0 for a console-only image)
#   BUNDLE_CLONEHERO  copy a locally cached official payload into the image
#                     (default: auto; set 0 for a redistributable image)
#   CLONEHERO_PAYLOAD_DIR  override the local extracted payload directory

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="$BUILD_DIR/out"
REPO_DIR="$BUILD_DIR/repo"
ROOTFS_DIR="$BUILD_DIR/rootfs"
ROOTFS_IMG="$OUT_DIR/rootfs.ext4"
ROOTFS_SIZE="${ROOTFS_SIZE:-8G}"
AMPKG="${AMPKG:-$ROOT_DIR/tools/ampkg/ampkg}"
INSTALL_BACKSTAGE="${INSTALL_BACKSTAGE:-1}"
BUNDLE_CLONEHERO="${BUNDLE_CLONEHERO:-auto}"
CLONEHERO_PAYLOAD_DIR="${CLONEHERO_PAYLOAD_DIR:-$BUILD_DIR/deps/clonehero-real/Linux - Standalone}"

case "$BUNDLE_CLONEHERO" in
    auto)
        if [ -x "$CLONEHERO_PAYLOAD_DIR/clonehero" ]; then
            BUNDLE_CLONEHERO=1
        else
            BUNDLE_CLONEHERO=0
        fi
        ;;
    0|1) ;;
    *)
        echo "!! BUNDLE_CLONEHERO must be auto, 0, or 1" >&2
        exit 2
        ;;
esac

mkdir -p "$OUT_DIR" "$ROOTFS_DIR"

[ -e "$OUT_DIR/busybox" ] || { echo "!! busybox not built yet — run scripts/build-busybox.sh (or make busybox)"; exit 1; }
[ -x "$AMPKG" ] || { echo "!! ampkg not built yet — run 'make ampkg'"; exit 1; }
[ -x "$OUT_DIR/backstage" ] || bash "$ROOT_DIR/scripts/build-backstage.sh"
[ -x "$OUT_DIR/dwm" ] && [ -x "$OUT_DIR/st" ] || bash "$ROOT_DIR/scripts/build-desktop.sh"

echo "==> building init (static)"
gcc -static -O2 -o "$OUT_DIR/init" "$ROOT_DIR/init/init.c"

echo "==> building packages"
bash "$ROOT_DIR/scripts/stage-tools.sh"
"$AMPKG" build packages/busybox packages/ghl-init packages/host-runtime packages/ghl-bootloader packages/fastfetch packages/ghl-installer packages/git packages/curl packages/nano packages/btop packages/jq packages/fd packages/clonehero packages/ghl-wayland packages/desktop packages/backstage packages/base -o "$REPO_DIR" -src "$ROOT_DIR"
"$AMPKG" repo-add "$REPO_DIR"

echo "==> installing base into the rootfs"
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"
"$AMPKG" -repo "$REPO_DIR" -r "$ROOTFS_DIR" install base
if [ "$INSTALL_BACKSTAGE" = 1 ]; then
    echo "==> enabling Backstage"
    "$AMPKG" -repo "$REPO_DIR" -r "$ROOTFS_DIR" install backstage
fi

if [ "$BUNDLE_CLONEHERO" = 1 ]; then
    if [ ! -x "$CLONEHERO_PAYLOAD_DIR/clonehero" ]; then
        echo "!! no verified Clone Hero payload at: $CLONEHERO_PAYLOAD_DIR"
        echo "   Install it in GHL with 'clonehero install', or set CLONEHERO_PAYLOAD_DIR."
        exit 1
    fi
    echo "==> bundling locally supplied Clone Hero payload"
    mkdir -p "$ROOTFS_DIR/opt/clonehero" "$ROOTFS_DIR/root/Clone Hero/Songs"
    cp -a "$CLONEHERO_PAYLOAD_DIR/." "$ROOTFS_DIR/opt/clonehero/"
fi

cat >"$OUT_DIR/rootfs-build.env" <<EOF
GHL_VERSION=0.1.0
GHL_CHANNEL=first-set
BUNDLE_CLONEHERO=$BUNDLE_CLONEHERO
EOF

echo "==> bundling ampkg + repo into the rootfs"
mkdir -p "$ROOTFS_DIR/usr/bin" "$ROOTFS_DIR/usr/share/ampkg" "$ROOTFS_DIR/boot"
cp "$AMPKG" "$ROOTFS_DIR/usr/bin/ampkg"
cp -a "$REPO_DIR/." "$ROOTFS_DIR/usr/share/ampkg/repo/"
[ -f "$OUT_DIR/bzImage" ] && cp "$OUT_DIR/bzImage" "$ROOTFS_DIR/boot/bzImage"
[ -f "$OUT_DIR/initramfs.cpio.gz" ] && cp "$OUT_DIR/initramfs.cpio.gz" "$ROOTFS_DIR/boot/initramfs.cpio.gz"

echo "==> packing rootfs image ($ROOTFS_IMG)"
rm -f "$ROOTFS_IMG"
mke2fs -q -t ext4 -d "$ROOTFS_DIR" "$ROOTFS_IMG" "$ROOTFS_SIZE"

echo "==> done: $ROOTFS_IMG"
