# Guitar Hero Linux

The dumbest possible way to play Clone Hero:

1. Buy a PS2 Gibson Guitar Hero guitar
2. Mod it for Clone Hero
3. Paint it like the Cinderstring from *Fisch*
4. Cram a mini PC inside it
5. Install GHL
6. Plug it into a TV
7. Play Clone Hero

Instead of plugging a guitar into a PC, **the guitar is the PC.**

GHL is a tiny Linux built from scratch on the kernel — no Buildroot, no Alpine,
no Yocto, no systemd. Just the kernel and a hand-rolled userspace, inspired by
Tiny Core Linux. After a one-time setup screen, it boots into **Backstage**, a controller-first game shell that
launches Clone Hero, system checks, installers, and a tiny emergency desktop.
Its bright Home Set design combines living-room channel cards with fast,
horizontal console navigation, smooth focus motion, sliding pages, and eased
dialogs while keeping GHL's own stage identity.

## Where things live

```
art/        logos and junk
docs/       design notes and roadmap
init/       the boot process
kernel/     kernel config
packages/   recipes for ampkg
scripts/    build scripts
tools/ampkg the command-line package manager (Go)
```

## Status

**0.1 Encore / First Set is playable in QEMU.** The kernel boots through the
hand-written init into an ampkg-built root filesystem, networking comes up over
DHCP, first boot opens the guided installer, Backstage then owns the display,
and the official Linux Clone Hero build runs
with keyboard/controller input and audio support. Desktop Mode now provides a
persistent WM, launcher, named workspaces and recovery tools; the installer can
prepare existing partitions and finish BIOS or UEFI GRUB installation. The root
shell also includes `ghl`, `ampkg`, Git, Nano, Fastfetch, and Curl.

Useful commands inside GHL:

```sh
ghl status       # release and game status
ghl doctor       # display, audio, network, input and payload checks
backstage        # start the game shell
clonehero run    # launch the game directly
installer        # install GHL to another disk
```

## Building

```sh
make            # kernel + initramfs
make qemu       # build and boot in QEMU (initramfs shell)
make qemu-root  # build + rootfs image, boot the real on-disk system
make iso        # build the small bootable recovery/initramfs ISO
make check      # run the complete 0.1 release gate
make release    # validate + assemble build/release/ghl-0.1.0-x86_64-first-set
```

The official Clone Hero payload is never downloaded silently. Install it from
inside GHL with `clonehero install`. Once a verified local payload exists in
the build cache, later builds retain it automatically. To explicitly include
another locally supplied payload, use:

```sh
BUNDLE_CLONEHERO=1 make release
```

Use `BUNDLE_CLONEHERO=0 make release` when producing a redistributable image
that contains the launcher but not the third-party game files.
Release kits containing the local payload receive a `-personal` suffix so they
cannot be confused with the public-safe artifact.

See [the 0.1 release notes](docs/releases/0.1.0.md) for controls, known limits,
and what is included.

The First Set release kit is the complete playable QEMU image. `ghl.iso` is a
recovery shell for now; it is not yet the full graphical installer media.
The release target also creates a portable `.tar.zst` archive and adjacent
`.sha256` file under `build/release/`.

See `scripts/README.md` and `docs/roadmap.md` for details.

## Disclaimer

Unofficial fan project. Not affiliated with Activision, RedOctane, Harmonix, Clone Hero, Roblox, or the Fisch devs.
