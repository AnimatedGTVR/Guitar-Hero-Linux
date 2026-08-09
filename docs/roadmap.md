# Roadmap

Rough plan, in order. Things will change as we actually do them.

## Phase 0 — Foundations (now)

- [x] Idea + README
- [x] Repo structure
- [x] `ampkg` scaffold (Go TUI)
- [x] Boot flow (BIOS bootloader + initramfs, `rdinit=/init`)
- [x] Minimal init (`init/init.c`)
- [x] Kernel config + build script
- [x] First boot to the GHL init (~400ms to init under QEMU/KVM)
- [ ] Shell in the initramfs

## Phase 1 — ampkg

- [ ] Recipe format (`packages/`)
- [ ] `.ampkg` archive format
- [ ] Local repo + index
- [ ] TUI: search / install / remove / upgrade
- [ ] Dependencies
- [ ] `base` meta-package

## Phase 2 — Userspace

- [ ] Shell + coreutils (busybox? toybox? own?)
- [ ] libc choice
- [ ] Drivers/firmware for the mini PC
- [ ] Network (downloading songs is a feature)

## Phase 3 — Clone Hero

- [ ] Wine/Proton runtime packaged
- [ ] Boot straight into Clone Hero
- [ ] Guitar input mapping
- [ ] HDMI/display handling
- [ ] Boot splash, shutdown, recovery mode

## Phase 4 — The actual guitar

- [ ] Pick a mini PC, mount it inside the guitar
- [ ] Power (battery? 12V?)
- [ ] Cinderstring paint job
- [ ] Test on the real thing
