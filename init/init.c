/*
 * GHL init — the very first process.
 *
 * For now it just brings up the essential mounts, says hello, and tries to
 * hand off to a shell. Eventually this becomes the tiny supervisor that
 * launches Clone Hero straight into the framebuffer.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static void mount_or(const char *dev, const char *dir, const char *type,
                     unsigned long flags, const char *note)
{
	if (mount(dev, dir, type, flags, NULL) != 0)
		perror(note);
}

static void setup_filesystems(void)
{
	mkdir("/proc", 0555);
	mkdir("/sys", 0555);
	mkdir("/tmp", 0777);
	mkdir("/dev", 0755);

	mount_or("proc", "/proc", "proc", 0, "GHL: mount proc");
	mount_or("sysfs", "/sys", "sysfs", 0, "GHL: mount sysfs");
	mount_or("tmpfs", "/tmp", "tmpfs", 0, "GHL: mount tmpfs");

	/* The kernel auto-mounts devtmpfs when CONFIG_DEVTMPFS_MOUNT=y; only
	 * fall back to a bare console node if it's not there. */
	if (access("/dev/console", F_OK) != 0) {
		mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
		if (access("/dev/console", F_OK) != 0)
			mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
	}
}

int main(void)
{
	sethostname("ghl", 3);

	printf("\n");
	printf("  GHL \x1b[1;36mGuitar Hero Linux\x1b[0m — the guitar is the PC.\n");
	printf("  kernel booted, init running as pid 1.\n\n");

	setup_filesystems();

	/* Hand off to a shell once one exists. Until then, hang around. */
	char *const argv[] = { "/bin/sh", NULL };
	char *const envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
		NULL,
	};

	execve("/bin/sh", argv, envp);

	printf("GHL: no shell yet, idling as pid 1.\n");
	for (;;)
		sleep(60);
}
