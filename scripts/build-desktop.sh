#!/bin/bash
set -euo pipefail

# Builds dwm and st (suckless, vendored under desktop/) for the "Desktop"
# item in Backstage. Uses pkg-config instead of each project's config.mk
# hardcoded /usr/X11R6 paths, which don't exist on Arch.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
mkdir -p "$OUT"

X11_FLAGS="$(pkg-config --cflags --libs x11 xinerama)"
FT_FLAGS="$(pkg-config --cflags --libs xft fontconfig freetype2)"

echo "==> building dwm"
cc -O2 -std=c99 -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L \
    -DVERSION=\"6.5\" -DXINERAMA \
    "$ROOT_DIR/desktop/dwm/drw.c" "$ROOT_DIR/desktop/dwm/dwm.c" "$ROOT_DIR/desktop/dwm/util.c" \
    -I"$ROOT_DIR/desktop/dwm" $X11_FLAGS $FT_FLAGS \
    -o "$OUT/dwm"

echo "==> building st"
cc -O2 -std=c99 -D_XOPEN_SOURCE=600 \
    -DVERSION=\"0.9.2\" \
    "$ROOT_DIR/desktop/st/st.c" "$ROOT_DIR/desktop/st/x.c" \
    -I"$ROOT_DIR/desktop/st" $X11_FLAGS $FT_FLAGS -lutil -lm -lrt \
    -o "$OUT/st"

echo "==> building ghl-bg"
cc -O2 "$ROOT_DIR/desktop/ghl-bg.c" $X11_FLAGS -o "$OUT/ghl-bg"

echo "==> done: $OUT/dwm $OUT/st $OUT/ghl-bg"
