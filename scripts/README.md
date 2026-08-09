# scripts

Build and maintenance scripts. Everything dumps output into `build/` (gitignored).

- `build-kernel.sh` — downloads the pinned kernel, applies the GHL config, builds `build/out/bzImage`
- `build-initramfs.sh` — builds static `init` and packs `build/out/initramfs.cpio.gz`
- `run.sh` — boots the latest build in QEMU (KVM when available)
- `make-iso.sh` — a bootable image (not written yet)

## Try it

```sh
./scripts/build-kernel.sh
./scripts/build-initramfs.sh
./scripts/run.sh
```
