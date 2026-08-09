#!/bin/bash
set -euo pipefail

# Build the GHL initramfs: static /init plus busybox (shell + coreutils)
# and the empty mount points init creates at boot.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
OUT_DIR="$BUILD_DIR/out"
STAGE="$BUILD_DIR/initramfs"

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/dev" "$STAGE/proc" "$STAGE/sys" "$STAGE/tmp" "$OUT_DIR"

echo "==> building init (static)"
gcc -static -O2 -o "$STAGE/init" "$ROOT_DIR/init/init.c"

if [ ! -e "$OUT_DIR/busybox" ]; then
    echo "!! busybox not built yet — run scripts/build-busybox.sh (or make busybox)"
    exit 1
fi

echo "==> installing busybox"
cp "$OUT_DIR/busybox" "$STAGE/bin/busybox"
for a in sh ls cat echo mount umount cp mv rm mkdir rmdir pwd chmod chown \
         sleep ps kill grep sed awk touch head tail wc find dd true false \
         dmesg clear halt poweroff reboot uname printf test hostname free \
         df du which env sort cut tr xargs ln tar gzip gunzip bzcat zcat \
         ping wget ifconfig udhcpc telnet su login vi nice taskset \
         uuencode uudecode dnsdomainname ftpget; do
    ln -s busybox "$STAGE/bin/$a"
done

echo "==> packing initramfs"
cd "$STAGE"
find . -print0 | cpio --null -o -H newc 2>/dev/null | gzip -9 > "$OUT_DIR/initramfs.cpio.gz"

echo "==> done: $OUT_DIR/initramfs.cpio.gz"
