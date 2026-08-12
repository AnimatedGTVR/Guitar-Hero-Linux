# Roadmap

Rough plan, in order. Things will change as we actually do them.

## Phase 0 — Foundations (now)

- [x] Idea + README
- [x] Repo structure
- [x] `ampkg` scaffold (Go CLI)
- [x] Boot flow (BIOS bootloader + initramfs, `rdinit=/init`)
- [x] Minimal init (`init/init.c`)
- [x] Kernel config + build script
- [x] First boot to the GHL init (~400ms to init under QEMU/KVM)
- [x] Bootable BIOS+UEFI ISO (`make iso`)
- [x] Shell in the initramfs (busybox ash + coreutils, static)
- [x] Real on-disk root filesystem: init mounts a root device from `root=`,
      moves it onto `/` (`MS_MOVE`) and hands off to `/sbin/init`
      (`make qemu-root`, rootfs image built from `packages/` with ampkg)
- [x] init as a tiny supervisor: one-shot `/etc/initrc`, reaps children,
      powers off when the shell exits
- [x] Network: DHCP on the NIC from `/etc/initrc` (udhcpc + default.script),
      verified in QEMU (lease, default route, DNS, ping)

## Phase 1 — ampkg

- [x] Recipe format (`packages/`)
- [x] `.ampkg` archive format
- [x] Local repo + index (`repo-add`)
- [x] CLI: search / install / remove / update / upgrade
- [x] Dependencies
- [x] `base` meta-package
- [x] Separate system installer command (mounted-target base installation)

## Phase 2 — Userspace

- [x] Shell + coreutils (busybox, static)
- [x] Network on the rootfs (DHCP + DNS via `initrc`)
- [x] libc choice: glibc for hosted tools; static BusyBox/init for bootstrap
- [x] Identified root environment (`os-release`, passwd/group, `/root`, prompt)
- [ ] Drivers/firmware for the mini PC
- [ ] Song downloads over that network

## Phase 3 — Clone Hero

- [x] Native Linux game downloader/launcher with checksum + EULA gate
- [x] Platform doctor for DRM, ALSA, display session, and evdev controllers
- [x] Backstage controller-first shell with flat First Set interface
- [x] Gamescope session package (native Wayland + XWayland)
- [x] Compatible Xorg/llvmpipe session validated inside GHL/QEMU
- [x] Native Unity runtime libraries validated on GHL
- [ ] Boot straight into Clone Hero
- [ ] Guitar input mapping
- [x] QEMU display, keyboard, mouse, controller, and fullscreen handoff
- [ ] Boot splash, shutdown, recovery mode

## 0.1 Encore — First Set

- [x] Playable Clone Hero session from a cold GHL boot
- [x] Backstage Play, Library, System, diagnostics, and Power screens
- [x] Working handoff to game installer, GHL installer, and Desktop Mode
- [x] Persistent Desktop Mode WM with launcher, named workspaces and tools
- [x] Guided target installer with profiles, verification, BIOS and UEFI GRUB
- [x] `ghl` status/doctor/logs/recovery command
- [x] Versioned release metadata and reproducible release-kit target
- [x] UUID/LABEL root discovery for first boot after installation
- [x] Automated release gate for source, packages, initramfs and ext4 image
- [x] Generate checksums and a self-verifying First Set release directory
- [ ] Validate on the first physical mini PC and television
- [ ] Publish the first downloadable image and its checksum

## Phase 4 — The actual guitar

- [ ] Pick a mini PC, mount it inside the guitar
- [ ] Power (battery? 12V?)
- [ ] Cinderstring paint job
- [ ] Test on the real thing
