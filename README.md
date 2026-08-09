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

GHL is a tiny Linux built from scratch on the kernel — no Buildroot, no Alpine, no Yocto, no systemd. Just the kernel and a hand-rolled userspace, inspired by Tiny Core Linux. It boots straight into Clone Hero. No desktop, no launcher, no bloat.

## Where things live

```
art/        logos and junk
docs/       design notes and roadmap
init/       the boot process
kernel/     kernel config
packages/   recipes for ampkg
scripts/    build scripts
tools/ampkg the package manager (Go TUI)
```

## Status

Very early. Mostly ideas and experiments that'll probably get rewritten a bunch.

## Disclaimer

Unofficial fan project. Not affiliated with Activision, RedOctane, Harmonix, Clone Hero, Roblox, or the Fisch devs.
