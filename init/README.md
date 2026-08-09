# init

What runs right after the kernel: the init system. No systemd, no display manager.

Current state: `init.c` sets the hostname to `ghl`, mounts proc/sys/tmp/dev,
prints a hello, and hands off to `/bin/sh` (busybox ash). The initramfs is a
full working shell: you can boot and type commands. Under QEMU/KVM the shell
appears in under half a second.

The plan, in order:

1. Boot from a minimal initramfs (done)
2. A shell you can actually use (done — busybox, static)
3. init mounts a real root filesystem and pivots out of the initramfs
4. a tiny supervisor starts the few services we need (network, Clone Hero)
5. Clone Hero takes over the screen

After a real root filesystem, the initramfs becomes a thin bootstrap layer
that hands off to the real system.
