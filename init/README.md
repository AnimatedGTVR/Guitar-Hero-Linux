# init

What runs right after the kernel: the init system. No systemd, no display manager.

Current state: `init.c` mounts proc/sys/tmp/dev, prints a hello, and idles as
pid 1 (or hands off to `/bin/sh` if one exists). First boot works — boot the
GHL kernel with the initramfs and you'll see it in under half a second.

The plan, in order:

1. Boot from a minimal initramfs (done)
2. init sets up device nodes, mounts the real root, pivots
3. a tiny supervisor starts the few services we need (network, Clone Hero)
4. Clone Hero takes over the screen

First milestone was boot to init (done). Next: a shell, then a real root
filesystem, then Clone Hero.
