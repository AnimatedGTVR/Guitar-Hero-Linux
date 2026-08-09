# kernel

Kernel config + build script for GHL, tuned for the mini PC that's going inside the guitar, plus whatever makes development on a regular PC easier.

`kernel/config` is a fragment layered on top of `make tinyconfig` — a nearly empty config — so the kernel only contains what the guitar needs. Current build: **1.8 MB** bzImage, boots to init in ~400ms under QEMU/KVM.

`scripts/build-kernel.sh` downloads the pinned kernel (6.12.x LTS), applies the config, builds, and drops the result in `build/out/bzImage`.

## Gotchas we've already hit

- `tinyconfig` produces a **32-bit** x86 kernel unless `CONFIG_64BIT=y`. A 64-bit init then fails with `ENOEXEC` (`Failed to execute /init (error -8)`). The fix is in `kernel/config` with a comment.
- If the initramfs has no shell, exec of `/bin/sh` fails silently and init idles.

Rule: only build what the guitar needs. Small kernel = fast boot.
