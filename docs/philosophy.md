# How this thing is built

Loose rules I'm trying to stick to. Not laws — just how I want this to feel.

- **From scratch.** Kernel + hand-written userspace. No Buildroot, no Alpine, no Yocto. Half the fun is building it ourselves, even when it's more work.
- **Small.** No desktop environment, no systemd, no junk. If it doesn't help you play Clone Hero it doesn't ship.
- **The guitar is the PC.** Everything lives inside a guitar, so: tiny footprint, fast boot, and nothing that depends on a mouse or full keyboard.
- **One job.** Clone Hero is the product. The kernel, init, and package manager are plumbing and should stay out of the way.
- **Hackable.** Keep the source small enough that someone can understand the whole thing in an afternoon.

If any component gets too complicated, that's a sign there's a dumber way. Take it.

Not doing: general-purpose distro stuff, supporting random hardware, anything that needs a mouse or a desktop.
