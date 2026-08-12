# packages

Recipes for everything GHL is made of. `ampkg` turns these into installable `.ampkg` archives.

Currently:

- `base` — the bootable root shell; pulls in BusyBox, init, and Fastfetch
- `busybox` — the userspace (shell, coreutils, networking) and its applet symlinks
- `ghl-init` — the static init binary as `/sbin/init`
- `git` — Git CLI, HTTPS transport, TLS roots, and its runtime libraries
- `fastfetch` — system information with GHL guitar ASCII art
- `host-runtime` — shared libraries and TLS roots used by hosted tools
- `ghl-installer` — GHL system setup UI exposed as `installer`
- `ghl-bootloader` — GRUB BIOS/UEFI tools used by the guided installer
- `curl`, `nano`, `btop`, `jq`, `fd` — optional terminal tools in the repository
- `clonehero` — official game downloader/launcher and platform diagnostics
- `ghl-wayland` — standalone Gamescope Wayland/XWayland session
- `desktop` — persistent dwm workbench, launcher, terminal and recovery tools
- `backstage` — controller-first GHL game shell and system dashboard
