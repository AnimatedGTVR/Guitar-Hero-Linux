#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT_DIR="$ROOT_DIR/build/out"
VERSION="${GHL_VERSION:-0.1.0}"
ROOTFS_BUNDLED=0
if [ -r "$OUT_DIR/rootfs-build.env" ]; then
    . "$OUT_DIR/rootfs-build.env"
    ROOTFS_BUNDLED="${BUNDLE_CLONEHERO:-0}"
fi
VARIANT="${GHL_VARIANT:-}"
if [ "$ROOTFS_BUNDLED" = 1 ] && [ -z "$VARIANT" ]; then
    VARIANT=personal
fi
NAME="ghl-$VERSION-x86_64-first-set${VARIANT:+-$VARIANT}"
RELEASE_DIR="$ROOT_DIR/build/release/$NAME"
ARCHIVE="$ROOT_DIR/build/release/$NAME.tar.zst"
ARCHIVE_CHECKSUM="$ARCHIVE.sha256"
BUILD_MESSAGE="$ROOT_DIR/build/release/$NAME-BUILD-MESSAGE.md"
RELEASE_ARCHIVE="${RELEASE_ARCHIVE:-1}"

for artifact in bzImage initramfs.cpio.gz rootfs.ext4; do
    [ -f "$OUT_DIR/$artifact" ] || {
        echo "!! missing $OUT_DIR/$artifact — run 'make release'" >&2
        exit 1
    }
done

echo "==> assembling $NAME"
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/build/out" "$RELEASE_DIR/scripts" "$RELEASE_DIR/docs"
cp "$OUT_DIR/bzImage" "$OUT_DIR/initramfs.cpio.gz" "$RELEASE_DIR/build/out/"
cp --sparse=always "$OUT_DIR/rootfs.ext4" "$RELEASE_DIR/build/out/rootfs.ext4"
cp "$ROOT_DIR/scripts/run-root.sh" "$RELEASE_DIR/scripts/"
cp "$ROOT_DIR/docs/releases/0.1.0.md" "$RELEASE_DIR/RELEASE-NOTES.md"
cp "$ROOT_DIR/README.md" "$RELEASE_DIR/README.md"
cp "$OUT_DIR/rootfs-build.env" "$RELEASE_DIR/build/out/" 2>/dev/null || true

cat >"$RELEASE_DIR/verify.sh" <<'EOF'
#!/bin/sh
set -eu
cd "$(dirname "$0")/build/out"
sha256sum -c SHA256SUMS
EOF
chmod +x "$RELEASE_DIR/verify.sh"

(
    cd "$RELEASE_DIR/build/out"
    sha256sum bzImage initramfs.cpio.gz rootfs.ext4 > SHA256SUMS
)

if [ "$ROOTFS_BUNDLED" = 1 ]; then
        echo "!! this personal release contains a locally supplied Clone Hero payload"
        echo "   Do not redistribute it as an official GHL image."
fi

if [ "$RELEASE_ARCHIVE" = 1 ]; then
    command -v zstd >/dev/null 2>&1 || {
        echo '!! zstd is required to create the release archive' >&2
        exit 1
    }
    echo "==> compressing $NAME.tar.zst"
    rm -f "$ARCHIVE" "$ARCHIVE_CHECKSUM" "$BUILD_MESSAGE"
    tar --sparse --sort=name --owner=0 --group=0 --numeric-owner \
        -C "$(dirname "$RELEASE_DIR")" -cf - "$NAME" |
        zstd -T0 -10 -q -o "$ARCHIVE"
    zstd -q -t "$ARCHIVE"
    (
        cd "$(dirname "$ARCHIVE")"
        sha256sum "$(basename "$ARCHIVE")" >"$(basename "$ARCHIVE_CHECKSUM")"
    )

    ARCHIVE_NAME="$(basename "$ARCHIVE")"
    ARCHIVE_SHA256="$(sha256sum "$ARCHIVE" | awk '{ print $1 }')"
    ARCHIVE_SIZE="$(du -h "$ARCHIVE" | awk '{ print $1 }')"
    if [ "$ROOTFS_BUNDLED" = 1 ]; then
        PAYLOAD_NOTE='This is a personal test build containing a locally supplied Clone Hero payload. Do not redistribute it as an official GHL download.'
    else
        PAYLOAD_NOTE='Clone Hero is not bundled. Use Install / Repair Game in Backstage and accept the upstream EULA to download it.'
    fi
    cat >"$BUILD_MESSAGE" <<EOF
# Guitar Hero Linux $VERSION Encore — First Set

The guitar is the PC. The first playable Guitar Hero Linux preview is ready.

## What is in this build

- a tiny hand-rolled Linux userspace with no systemd, Buildroot, Alpine, or Yocto
- a guided first-boot TUI installer with safe BIOS and UEFI installation flows
- Backstage, the animated controller-first home screen for Play, Songs, Tools, and Power
- automatic scaling for 4:3, 16:9, 16:10, portrait, HiDPI, and ultrawide displays
- keyboard, mouse, controller, left-stick, D-pad, and raw joystick navigation
- Clone Hero launch and repair tools, Desktop Mode, ampkg, Git, Nano, Curl, and Fastfetch
- QEMU/KVM boot scripts plus an integrated release verification command

## Try it

1. Download \`$ARCHIVE_NAME\` and its adjacent \`.sha256\` file.
2. Run \`sha256sum -c $ARCHIVE_NAME.sha256\`.
3. Extract it with \`tar --use-compress-program=unzstd -xf $ARCHIVE_NAME\`.
4. Enter the extracted directory, run \`./verify.sh\`, then \`./scripts/run-root.sh\`.
5. First boot opens the installer. Choose **Exit to Backstage / terminal** when you are ready to enter Backstage.

Backstage controls: arrows, WASD/HJKL, D-pad, left stick, or mouse to navigate; Enter/A/left-click selects; Escape/B/right-click goes back.

## Build information

- Artifact: \`$ARCHIVE_NAME\`
- Size: $ARCHIVE_SIZE
- SHA-256: \`$ARCHIVE_SHA256\`
- Source: https://github.com/AnimatedGTVR/Guitar-Hero-Linux

$PAYLOAD_NOTE

GHL is an unofficial fan project and is not affiliated with Activision, RedOctane, Harmonix, or Clone Hero.
EOF
fi

echo "==> release kit ready: $RELEASE_DIR"
echo "    boot it with: $RELEASE_DIR/scripts/run-root.sh"
if [ "$RELEASE_ARCHIVE" = 1 ]; then
    echo "    archive: $ARCHIVE"
    echo "    checksum: $ARCHIVE_CHECKSUM"
    echo "    build message: $BUILD_MESSAGE"
fi
