#!/bin/bash
set -euo pipefail

# Release-gate checks for the complete GHL 0.1 image.  These intentionally
# inspect both the staging tree and the packed ext4 image so a successful
# package build cannot hide a broken release artifact.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
ROOTFS="$ROOT_DIR/build/rootfs"
AMPKG="$ROOT_DIR/tools/ampkg/ampkg"
TMP_DIR="$(mktemp -d -t ghl-release-check.XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

say() { printf '==> check: %s\n' "$*"; }
die() { printf '!! release check failed: %s\n' "$*" >&2; exit 1; }

for artifact in bzImage initramfs.cpio.gz rootfs.ext4; do
    [ -s "$OUT/$artifact" ] || die "missing build/out/$artifact"
done
[ -x "$AMPKG" ] || die 'ampkg binary is missing'

say 'source syntax and compiler warnings'
bash -n "$ROOT_DIR"/scripts/*.sh
while IFS= read -r -d '' file; do
    case "$(head -n 1 "$file")" in
        '#!/bin/sh'*) sh -n "$file" ;;
        '#!/bin/bash'*) bash -n "$file" ;;
    esac
done < <(find "$ROOT_DIR/packages" -type f -print0)
gcc -static -O2 -Wall -Wextra -Werror \
    -o "$TMP_DIR/init" "$ROOT_DIR/init/init.c"
gcc -fsyntax-only -Wall -Wextra -Werror \
    $(pkg-config --cflags sdl2) "$ROOT_DIR/backstage/backstage.c"

say 'ampkg unit tests'
(cd "$ROOT_DIR/tools/ampkg" && go test ./...)

say 'root filesystem contents'
required_paths=(
    bin/sh sbin/init etc/os-release etc/profile etc/ghl-release
    usr/bin/ampkg usr/bin/backstage usr/bin/backstage-ui
    usr/bin/clonehero usr/bin/ghl usr/bin/installer usr/bin/ghl-desktop
    usr/bin/dwm usr/bin/st usr/bin/fastfetch usr/bin/sudo
    usr/bin/findfs usr/bin/killall usr/bin/uptime
    boot/bzImage boot/initramfs.cpio.gz
)
for path in "${required_paths[@]}"; do
    [ -e "$ROOTFS/$path" ] || die "rootfs is missing /$path"
done
grep -q '/var/lib/ghl/first-boot-complete' "$ROOTFS/etc/profile" ||
    die 'login profile has no first-boot installer gate'
[ ! -e "$ROOTFS/var/lib/ghl/first-boot-complete" ] ||
    die 'fresh rootfs was incorrectly marked as already configured'

if [ -r "$OUT/rootfs-build.env" ]; then
    . "$OUT/rootfs-build.env"
    if [ "${BUNDLE_CLONEHERO:-0}" = 1 ]; then
        [ -x "$ROOTFS/opt/clonehero/clonehero" ] ||
            die 'build is marked playable but the Clone Hero payload is missing'
        CLONEHERO_HOME="$ROOTFS/opt/clonehero" \
            sh "$ROOTFS/usr/bin/clonehero" status >/dev/null ||
            die 'bundled Clone Hero payload failed its launcher status check'
    fi
fi

installed="$($AMPKG -r "$ROOTFS" list)"
for package in base backstage busybox clonehero desktop ghl-installer ghl-init; do
    grep -q "^$package " <<<"$installed" || die "package database is missing $package"
done

say 'initramfs stable-root support'
gzip -dc "$OUT/initramfs.cpio.gz" | cpio -it 2>/dev/null >"$TMP_DIR/initramfs.list"
grep -Eq '^\.?/?bin/findfs$' "$TMP_DIR/initramfs.list" || die 'initramfs has no findfs applet'
grep -Eq '^\.?/?init$' "$TMP_DIR/initramfs.list" || die 'initramfs has no init'

say 'packed ext4 filesystem'
e2fsck -fn "$OUT/rootfs.ext4" >"$TMP_DIR/e2fsck.log" 2>&1 || {
    cat "$TMP_DIR/e2fsck.log" >&2
    die 'rootfs.ext4 did not pass e2fsck'
}
for path in /sbin/init /usr/bin/backstage /usr/bin/installer /boot/bzImage; do
    debugfs -R "stat $path" "$OUT/rootfs.ext4" >/dev/null 2>&1 ||
        die "packed image is missing $path"
done
if debugfs -R 'stat /var/lib/ghl/first-boot-complete' "$OUT/rootfs.ext4" 2>/dev/null |
        grep -q '^Inode:'; then
    die 'packed image was incorrectly marked as already configured'
fi

if command -v git >/dev/null 2>&1 && git -C "$ROOT_DIR" rev-parse --git-dir >/dev/null 2>&1; then
    say 'patch whitespace'
    git -C "$ROOT_DIR" diff --check
fi

printf '\nGHL 0.1 release checks passed.\n'
