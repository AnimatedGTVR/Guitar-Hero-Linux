#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
mkdir -p "$ROOT_DIR/build/out"
cc -O2 -Wall -Wextra "$ROOT_DIR/backstage/backstage.c" -o "$ROOT_DIR/build/out/backstage" $(pkg-config --cflags --libs sdl2) -lm
echo "==> done: $ROOT_DIR/build/out/backstage"
