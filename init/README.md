# init

What runs right after the kernel: the init system. No systemd, no display manager.

Current state: `init.c` is a tiny supervisor. It sets the hostname to `ghl`,
mounts proc/sys/tmp/dev, and does double duty:

- as `/init` in the initramfs it looks for a `root=` on the kernel command
  line, and if present mounts that device, moves it onto `/` (`MS_MOVE`), and
  hands off to `/sbin/init` on the real root;
- as `/sbin/init` on the real root it runs the one-shot `/etc/initrc`
  (network setup etc.), then spawns the interactive shell as a child. It
  reaps everything forever, and powers the box off when the shell exits.

The initramfs is still a full working shell (handy for recovery), but with a
root disk attached the boot hands off to the on-disk system.

The plan, in order:

1. Boot from a minimal initramfs (done)
2. A shell you can actually use (done — busybox, static)
3. init mounts a real root filesystem and hands off to it (done — `make qemu-root`)
4. a tiny supervisor starts the few services we need (done — `/etc/initrc`, network first)
5. Clone Hero takes over the screen

After a real root filesystem, the initramfs becomes a thin bootstrap layer
that hands off to the real system.
