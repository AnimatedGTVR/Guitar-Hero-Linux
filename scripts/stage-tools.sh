#!/bin/bash
set -euo pipefail

# Stage host-built tools separately from their shared runtime. This is an
# early-bootstrapping bridge until GHL has its own native toolchain builds.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
RUNTIME="$ROOT_DIR/build/host-runtime-root"
GIT_STAGE="$ROOT_DIR/build/git-root"
FASTFETCH_STAGE="$ROOT_DIR/build/fastfetch-root"
OPTIONAL_STAGE="$ROOT_DIR/build/optional-tools"
NANO_STAGE="$ROOT_DIR/build/nano-root"
WAYLAND_STAGE="$ROOT_DIR/build/wayland-root"
GRUB_STAGE="$ROOT_DIR/build/grub-root"
GIT_BIN="$(command -v git)"
FASTFETCH_BIN="$(command -v fastfetch)"
GIT_EXEC_PATH="$(git --exec-path)"
GAMESCOPE_BIN="$(command -v gamescope)"
XWAYLAND_BIN="$(command -v Xwayland)"
XORG_BIN="$(command -v Xorg)"
XORG_SERVER=/usr/lib/Xorg
XKBCOMP_BIN="$(command -v xkbcomp)"
GBM_BACKEND=/usr/lib/gbm/dri_gbm.so
GALLIUM_LIB="$(ldd "$GBM_BACKEND" | awk '/libgallium/ { print $3; exit }')"
SEATD_BIN="$(command -v seatd)"
BACKSTAGE_BIN="$ROOT_DIR/build/out/backstage"
DWM_BIN="$ROOT_DIR/build/out/dwm"
ST_BIN="$ROOT_DIR/build/out/st"
GHLBG_BIN="$ROOT_DIR/build/out/ghl-bg"
SDL3_LIB="$(ldconfig -p | awk '/libSDL3\.so\.0 / { print $NF; exit }')"
ALSA_LIB=/usr/lib/libasound.so.2
GTK3_LIB=/usr/lib/libgtk-3.so.0
SWRAST_CACHE="$ROOT_DIR/build/deps/vulkan-swrast.pkg.tar.zst"
SWRAST_ROOT="$ROOT_DIR/build/vulkan-swrast-root"
VIRTIO_CACHE="$ROOT_DIR/build/deps/vulkan-virtio.pkg.tar.zst"
VIRTIO_ROOT="$ROOT_DIR/build/vulkan-virtio-root"
EVDEV_CACHE="$ROOT_DIR/build/deps/xf86-input-evdev.pkg.tar.zst"
EVDEV_ROOT="$ROOT_DIR/build/xf86-input-evdev-root"
MKFS_FAT_BIN="$(command -v mkfs.fat)"
GRUB_BINS=(
    "$(command -v grub-install)"
    "$(command -v grub-mkimage)"
    "$(command -v grub-probe)"
    "$(command -v grub-bios-setup)"
    "$(command -v grub-mkrelpath)"
)

rm -rf "$RUNTIME" "$GIT_STAGE" "$FASTFETCH_STAGE" "$OPTIONAL_STAGE" "$NANO_STAGE" "$WAYLAND_STAGE" "$GRUB_STAGE"
mkdir -p "$RUNTIME/usr/lib" "$RUNTIME/etc/ssl/certs" "$RUNTIME/lib64"
mkdir -p "$GIT_STAGE/usr/bin" "$GIT_STAGE/usr/lib" "$GIT_STAGE/usr/share/git-core"
mkdir -p "$FASTFETCH_STAGE/usr/bin" "$FASTFETCH_STAGE/etc/fastfetch" "$FASTFETCH_STAGE/usr/share/fastfetch"
mkdir -p "$OPTIONAL_STAGE"
mkdir -p "$NANO_STAGE/usr/bin" "$NANO_STAGE/usr/share"
mkdir -p "$WAYLAND_STAGE/usr/bin" "$WAYLAND_STAGE/usr/lib" "$WAYLAND_STAGE/usr/share"
mkdir -p "$GRUB_STAGE/usr/bin" "$GRUB_STAGE/usr/lib" "$GRUB_STAGE/usr/share"
mkdir -p "$(dirname "$SWRAST_CACHE")" "$SWRAST_ROOT" "$VIRTIO_ROOT" "$EVDEV_ROOT"

# QEMU's basic virtio-vga has no Vulkan device. Bundle Mesa Lavapipe so the
# standalone shell still works without virgl/Venus or GPU passthrough.
if [ ! -s "$SWRAST_CACHE" ]; then
    SWRAST_URL="$(pacman -Sp --print-format '%l' vulkan-swrast | tail -n 1)"
    curl -fL "$SWRAST_URL" -o "$SWRAST_CACHE"
fi
bsdtar -xf "$SWRAST_CACHE" -C "$SWRAST_ROOT" usr/lib/libvulkan_lvp.so usr/share/vulkan/icd.d/lvp_icd.json
SWRAST_LIB="$SWRAST_ROOT/usr/lib/libvulkan_lvp.so"
if [ ! -s "$VIRTIO_CACHE" ]; then
    VIRTIO_URL="$(pacman -Sp --print-format '%l' vulkan-virtio | tail -n 1)"
    curl -fL "$VIRTIO_URL" -o "$VIRTIO_CACHE"
fi
bsdtar -xf "$VIRTIO_CACHE" -C "$VIRTIO_ROOT" usr/lib/libvulkan_virtio.so usr/share/vulkan/icd.d/virtio_icd.json
VIRTIO_LIB="$VIRTIO_ROOT/usr/lib/libvulkan_virtio.so"

# Xorg's libinput driver requires a populated udev database. GHL deliberately
# uses a much smaller devtmpfs-only userspace, so bundle the evdev driver for
# direct /dev/input/event* access instead.
if [ ! -s "$EVDEV_CACHE" ]; then
    EVDEV_URL="$(pacman -Sp --print-format '%l' xf86-input-evdev | tail -n 1)"
    curl -fL "$EVDEV_URL" -o "$EVDEV_CACHE"
fi
bsdtar -xf "$EVDEV_CACHE" -C "$EVDEV_ROOT" usr/lib/xorg/modules/input/evdev_drv.so
EVDEV_DRIVER="$EVDEV_ROOT/usr/lib/xorg/modules/input/evdev_drv.so"

cp -a "$GIT_BIN" "$GIT_STAGE/usr/bin/git"
cp -a "$GIT_EXEC_PATH" "$GIT_STAGE/usr/lib/git-core"
if [ -d /usr/share/git-core/templates ]; then
    cp -a /usr/share/git-core/templates "$GIT_STAGE/usr/share/git-core/templates"
fi

cp -a "$FASTFETCH_BIN" "$FASTFETCH_STAGE/usr/bin/fastfetch"
cp "$ROOT_DIR/packages/fastfetch/config.jsonc" "$FASTFETCH_STAGE/etc/fastfetch/config.jsonc"
cp "$ROOT_DIR/packages/fastfetch/ghl-ascii.txt" "$FASTFETCH_STAGE/usr/share/fastfetch/ghl-ascii.txt"

OPTIONAL_TOOLS=(curl btop jq fd)
for tool in "${OPTIONAL_TOOLS[@]}"; do
    tool_path="$(command -v "$tool")"
    cp -aL "$tool_path" "$OPTIONAL_STAGE/$tool"
done
cp -aL "$(command -v nano)" "$NANO_STAGE/usr/bin/nano"
cp -a /usr/share/terminfo "$NANO_STAGE/usr/share/terminfo"

# The installer can finish both legacy BIOS and removable-path UEFI installs.
# Keep GRUB isolated in its own package while sharing the hosted glibc runtime.
for grub_binary in "${GRUB_BINS[@]}"; do
    cp -aL "$grub_binary" "$GRUB_STAGE/usr/bin/"
done
cp -aL "$MKFS_FAT_BIN" "$GRUB_STAGE/usr/bin/mkfs.fat"
cp -a /usr/lib/grub "$GRUB_STAGE/usr/lib/grub"
cp -a /usr/share/grub "$GRUB_STAGE/usr/share/grub"

cp -aL "$GAMESCOPE_BIN" "$WAYLAND_STAGE/usr/bin/gamescope"
cp -aL "$XWAYLAND_BIN" "$WAYLAND_STAGE/usr/bin/Xwayland"
cp -aL "$XORG_BIN" "$WAYLAND_STAGE/usr/bin/Xorg"
cp -aL "$XORG_SERVER" "$WAYLAND_STAGE/usr/lib/Xorg"
cp -aL "$XKBCOMP_BIN" "$WAYLAND_STAGE/usr/bin/xkbcomp"
mkdir -p "$WAYLAND_STAGE/usr/share/ghl"
"$XKBCOMP_BIN" -I/usr/share/X11/xkb -xkm "$ROOT_DIR/packages/ghl-wayland/default.xkb" "$WAYLAND_STAGE/usr/share/ghl/default.xkm" 2>/dev/null
cp -aL "$SEATD_BIN" "$WAYLAND_STAGE/usr/bin/seatd"
cp -a /usr/lib/xorg "$WAYLAND_STAGE/usr/lib/xorg"
cp -aL "$EVDEV_DRIVER" "$WAYLAND_STAGE/usr/lib/xorg/modules/input/evdev_drv.so"
mkdir -p "$WAYLAND_STAGE/usr/lib/gbm"
cp -aL "$GBM_BACKEND" "$WAYLAND_STAGE/usr/lib/gbm/dri_gbm.so"
cp -a /usr/lib/dri "$WAYLAND_STAGE/usr/lib/dri"
cp -a /usr/share/gamescope "$WAYLAND_STAGE/usr/share/gamescope"
mkdir -p "$WAYLAND_STAGE/usr/share/X11" "$WAYLAND_STAGE/usr/share/vulkan"
cp -aL /usr/share/X11/xkb "$WAYLAND_STAGE/usr/share/X11/xkb"
[ ! -d /usr/share/X11/xorg.conf.d ] || cp -a /usr/share/X11/xorg.conf.d "$WAYLAND_STAGE/usr/share/X11/xorg.conf.d"
cp -a /usr/share/vulkan/icd.d "$WAYLAND_STAGE/usr/share/vulkan/icd.d"
cp -aL "$SWRAST_LIB" "$WAYLAND_STAGE/usr/lib/libvulkan_lvp.so"
cp -a "$SWRAST_ROOT/usr/share/vulkan/icd.d/lvp_icd.json" "$WAYLAND_STAGE/usr/share/vulkan/icd.d/lvp_icd.json"
cp -aL "$VIRTIO_LIB" "$WAYLAND_STAGE/usr/lib/libvulkan_virtio.so"
cp -a "$VIRTIO_ROOT/usr/share/vulkan/icd.d/virtio_icd.json" "$WAYLAND_STAGE/usr/share/vulkan/icd.d/virtio_icd.json"
[ ! -d /usr/share/libinput ] || cp -a /usr/share/libinput "$WAYLAND_STAGE/usr/share/libinput"
cp -aL /usr/lib/libvulkan.so.1 "$WAYLAND_STAGE/usr/lib/libvulkan.so.1"
for driver in /usr/lib/libvulkan_*.so; do cp -aL "$driver" "$WAYLAND_STAGE/usr/lib/"; done

mapfile -t LIBS < <(
    { ldd "$GIT_BIN" "$GIT_EXEC_PATH/git-remote-http" "$FASTFETCH_BIN" "$(command -v nano)" "$GAMESCOPE_BIN" "$XWAYLAND_BIN" "$XORG_SERVER" "$XKBCOMP_BIN" "$GBM_BACKEND" "$GALLIUM_LIB" "$SEATD_BIN" "$BACKSTAGE_BIN" "$DWM_BIN" "$ST_BIN" "$GHLBG_BIN" "$SDL3_LIB" "$ALSA_LIB" "$GTK3_LIB" "$SWRAST_LIB" "$VIRTIO_LIB" "$EVDEV_DRIVER" "$MKFS_FAT_BIN" "${GRUB_BINS[@]}" /usr/lib/libEGL.so.1 /usr/lib/libEGL_mesa.so.0 /usr/lib/libGLESv2.so.2 /usr/lib/libGLX_mesa.so.0 /usr/lib/dri/virtio_gpu_dri.so /usr/lib/dri/kms_swrast_dri.so /usr/lib/libvulkan_intel.so; find /usr/lib/xorg/modules -type f -name '*.so' -exec ldd {} + 2>/dev/null; for tool in "${OPTIONAL_TOOLS[@]}"; do ldd "$(command -v "$tool")" 2>/dev/null || true; done; } |
        awk '/=> \// { print $3 } /^\// { print $1 }' |
        sort -u
)
for lib in "${LIBS[@]}"; do
    [ -e "$lib" ] || continue
    mkdir -p "$RUNTIME$(dirname "$lib")"
    cp -aL "$lib" "$RUNTIME$lib"
done

# Gamescope loads SDL3 with dlopen(), so ldd on Gamescope cannot discover it.
cp -aL "$SDL3_LIB" "$RUNTIME/usr/lib/libSDL3.so.0"
cp -aL "$ALSA_LIB" "$RUNTIME/usr/lib/libasound.so.2"
cp -aL "$GTK3_LIB" "$RUNTIME/usr/lib/libgtk-3.so.0"
cp -aL /usr/lib/libEGL.so.1 "$RUNTIME/usr/lib/libEGL.so.1"
cp -aL /usr/lib/libGLESv2.so.2 "$RUNTIME/usr/lib/libGLESv2.so.2"
cp -aL /usr/lib/libGLX_mesa.so.0 "$RUNTIME/usr/lib/libGLX_mesa.so.0"

# Clone Hero's bundled native MIDI and file-picker plugins link to these even
# though the small Unity bootstrap executable does not. Keep the corresponding
# data files alongside the libraries so ALSA can resolve the default device and
# GTK can initialize its settings backend.
mkdir -p "$RUNTIME/usr/share/alsa" "$RUNTIME/usr/share/glib-2.0/schemas"
cp -a /usr/share/alsa/. "$RUNTIME/usr/share/alsa/"
cp -a /usr/share/glib-2.0/schemas/. "$RUNTIME/usr/share/glib-2.0/schemas/"

# libEGL.so.1 here is the libglvnd dispatcher, not Mesa's real EGL — it
# dlopen()s its vendor at runtime based on this JSON, so ldd can never see
# the dependency. Without it, eglGetDisplay() fails with no vendor
# configured, glamor never initializes, and everything (Xorg's own GL,
# Clone Hero's Unity player) silently falls back to software rendering —
# which is also what was corrupting Unity's GPU memory-pool sizing.
mkdir -p "$RUNTIME/usr/share/glvnd/egl_vendor.d"
cp -aL /usr/share/glvnd/egl_vendor.d/50_mesa.json "$RUNTIME/usr/share/glvnd/egl_vendor.d/50_mesa.json"
cp -aL /usr/lib/libEGL_mesa.so.0 "$RUNTIME/usr/lib/libEGL_mesa.so.0"
cp -aL /usr/lib/libxcb-dri2.so.0 "$RUNTIME/usr/lib/libxcb-dri2.so.0"

# Third-party binaries GHL downloads at runtime (Clone Hero's Unity build)
# aren't around at build time for ldd to discover, and are usually linked
# against pre-glibc-2.34 sonames that no longer exist as real libraries —
# glibc merged them into libc.so.6 but still ships tiny compat shims under
# the old names. ldd can't find these ahead of time, so stage them
# unconditionally.
for compat in libpthread.so.0 libdl.so.2 librt.so.1 libnsl.so.1 libanl.so.1 libutil.so.1 libcrypt.so.1; do
    src="/usr/lib/$compat"
    [ -e "$src" ] || continue
    cp -aL "$src" "$RUNTIME/usr/lib/$compat"
done

LOADER="$(ldd "$GIT_BIN" | awk '/ld-linux/ { print $1; exit }')"
if [ -n "$LOADER" ] && [ -e "$LOADER" ]; then
    mkdir -p "$RUNTIME$(dirname "$LOADER")"
    cp -aL "$LOADER" "$RUNTIME$LOADER"
fi

CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
if [ -e "$CA_BUNDLE" ]; then
    cp -aL "$CA_BUNDLE" "$RUNTIME/etc/ssl/certs/ca-certificates.crt"
fi

# dwm's bar and st both render text through Xft/fontconfig, which needs a
# real config (not just the shared libs) to find anything. The default
# config already scans /usr/share/fonts, which is where the desktop
# package's bundled JetBrains Mono lands.
mkdir -p "$RUNTIME/etc/fonts" "$RUNTIME/var/cache/fontconfig"
cp -a /etc/fonts/. "$RUNTIME/etc/fonts/"

echo "==> staged Git, Fastfetch, Nano, GRUB, Backstage Wayland, ${OPTIONAL_TOOLS[*]}, and shared runtime"
