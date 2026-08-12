# scripts

Build and maintenance scripts. Everything dumps output into `build/` (gitignored).

- `build-kernel.sh` — downloads the pinned kernel, applies the GHL config, builds `build/out/bzImage`
- `build-busybox.sh` — downloads + builds static busybox (the userspace: shell, coreutils, networking)
- `build-initramfs.sh` — builds static `init`, installs busybox + applet symlinks, packs `build/out/initramfs.cpio.gz`
- `build-rootfs.sh` — builds `init` and the packages with ampkg, installs `base` into `build/rootfs`, packs `build/out/rootfs.ext4` (an ext4 disk image)
- `make-iso.sh` — builds a bootable BIOS+UEFI ISO with GRUB (config in `scripts/iso/grub.cfg`)
- `run.sh` — boots the latest build in QEMU (KVM when available), initramfs shell only
- `run-root.sh` — like `run.sh` but attaches the rootfs disk + a virtio NIC and boots the real on-disk system (`root=/dev/vda`, network via QEMU user networking)
- `make-release.sh` — stages the kernel, initramfs, sparse rootfs, runner and notes, then produces a `.tar.zst` archive with SHA-256 checksums
- `check-release.sh` — release gate for source warnings, ampkg tests, required files, initramfs contents, and ext4 integrity

The graphical runner presents a 1280×720 virtual display and scales it to fit
inside the QEMU window, so Backstage remains fully visible on smaller host
screens. The QEMU window can be resized freely.

The framebuffer console and QEMU scanout use the same resolution, preventing
the root terminal from being clipped or offset after Xorg exits. Override both
together when testing another shape:

```sh
QEMU_RESOLUTION=1920x1080 ./scripts/run-root.sh
```

Backstage uses a 1280×720 logical canvas but automatically scales it to the
real renderer output. The default `fit` mode preserves aspect ratio and adds
centered black bars for 4:3, 16:10, ultrawide, or portrait screens. Advanced
overrides are available when testing:

```sh
BACKSTAGE_SCALE=stretch backstage  # fill the output without letterboxing
BACKSTAGE_SCALE=integer backstage  # whole-number pixel scaling when possible
GHL_DISPLAY_WIDTH=1920 GHL_DISPLAY_HEIGHT=1080 backstage-session
```

Run the complete system as an interactive terminal-only VM:

```sh
./scripts/run-root.sh --console
```

This connects QEMU's serial port to the current terminal. Log in is automatic
and the shell has root access. Use `poweroff` for a clean shutdown or Ctrl-A X
to force QEMU to exit.

There's a Makefile so you usually don't need to call these directly:

```sh
make            # kernel + initramfs
make qemu       # build, then boot in QEMU (initramfs shell)
make qemu-root  # build + rootfs image, then boot the real on-disk system
make iso        # build the recovery/initramfs ISO at build/out/ghl.iso
make check      # validate the complete 0.1 root filesystem
make release    # validate and assemble the versioned first-set release directory
make clean      # remove build/
```

An already supplied local Clone Hero payload is retained automatically in
later personal builds. Set `BUNDLE_CLONEHERO=1` to require it, or
`BUNDLE_CLONEHERO=0` to produce a redistributable launcher-only image. GHL
never downloads the game implicitly. Bundled release kits are named
`first-set-personal`; launcher-only kits keep the normal `first-set` name.

Test the ISO in QEMU:

```sh
qemu-system-x86_64 -cdrom build/out/ghl.iso -serial stdio -display none
```

The GRUB config uses a dual console (VGA + serial), so the ISO works headless for debugging and on a real display.
The ISO currently contains the recovery initramfs; the complete graphical 0.1
system is the versioned First Set QEMU release kit.
