# scripts

Build and maintenance scripts. Everything dumps output into `build/` (gitignored).

- `build-kernel.sh` — downloads the pinned kernel, applies the GHL config, builds `build/out/bzImage`
- `build-busybox.sh` — downloads + builds static busybox (the userspace: shell, coreutils, networking)
- `build-initramfs.sh` — builds static `init`, installs busybox + applet symlinks, packs `build/out/initramfs.cpio.gz`
- `make-iso.sh` — builds a bootable BIOS+UEFI ISO with GRUB (config in `scripts/iso/grub.cfg`)
- `run.sh` — boots the latest build in QEMU (KVM when available)

There's a Makefile so you usually don't need to call these directly:

```sh
make            # kernel + initramfs
make qemu       # build, then boot in QEMU
make iso        # build, then make build/out/ghl.iso
make clean      # remove build/
```

Test the ISO in QEMU:

```sh
qemu-system-x86_64 -cdrom build/out/ghl.iso -serial stdio -display none
```

The GRUB config uses a dual console (VGA + serial), so the ISO works headless for debugging and on a real display.
