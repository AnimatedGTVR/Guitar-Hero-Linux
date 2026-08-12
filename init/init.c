/*
 * GHL init — the very first process.
 *
 * Two roles:
 *
 *  1. In the initramfs (rdinit=/init): bring up essential mounts, look for a
 *     real root on the kernel command line, mount it, move it onto /, and
 *     hand off to /sbin/init.
 *
 *  2. As /sbin/init on the real root: the tiny supervisor. It mounts the
 *     essentials, runs the one-shot setup (/etc/initrc — network, etc.), then
 *     spawns the interactive shell and reaps children. When the last console
 *     shell exits, it powers the box off.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

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

	/* Mount devtmpfs at /dev so block/input devices (the root disk, the
	 * guitar controller) get device nodes. The kernel only auto-mounts it
	 * when booting a real root directly; with an initramfs we do it here.
	 * Fall back to a bare console node if it can't be mounted. */
	if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0) {
		perror("GHL: mount devtmpfs");
		mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1));
	}

	/* Programs that allocate their own pseudoterminal (st, for the
	 * desktop session) need /dev/pts mounted — plain shells inheriting
	 * their console tty never touched this path, so it went unnoticed
	 * until something finally called openpty(). */
	mkdir("/dev/pts", 0755);
	if (mount("devpts", "/dev/pts", "devpts", 0, "gid=5,mode=620,ptmxmode=666") != 0)
		perror("GHL: mount devpts");
}

/* Read the kernel command line looking for the given parameter's value
 * ("root=/dev/sda" -> "/dev/sda"). Returns 0 if found, -1 otherwise. */
static int cmdline_value(const char *param, char *out, size_t outlen)
{
	int fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0)
		return -1;

	char buf[4096];
	ssize_t n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = '\0';

	char *p = buf;
	size_t plen = strlen(param);
	while (*p) {
		char *end = strchr(p, ' ');
		size_t token_len = end ? (size_t)(end - p) : strlen(p);
		if (token_len > plen && strncmp(p, param, plen) == 0 && p[plen] == '=') {
			const char *val = p + plen + 1;
			size_t vlen = end ? (size_t)(end - val) : strlen(val);
			if (vlen >= outlen)
				vlen = outlen - 1;
			memcpy(out, val, vlen);
			out[vlen] = '\0';
			/* /proc/cmdline has a trailing newline; trim it. */
			size_t olen = strlen(out);
			while (olen > 0 && (out[olen - 1] == '\n' || out[olen - 1] == '\r' ||
			                    out[olen - 1] == ' '))
				out[--olen] = '\0';
			return 0;
		}
		if (!end)
			break;
		p = end + 1;
		while (*p == ' ')
			p++;
	}
	return -1;
}

/* Resolve the stable root identifiers written by the installer.  GHL does
 * not run udev, so /dev/disk/by-uuid is not available in the initramfs.  The
 * static BusyBox findfs applet probes the block devices directly instead. */
static int resolve_root(const char *spec, char *out, size_t outlen)
{
	if (spec[0] == '/') {
		snprintf(out, outlen, "%s", spec);
		return 0;
	}
	if (strncmp(spec, "UUID=", 5) != 0 && strncmp(spec, "LABEL=", 6) != 0)
		return -1;

	int pipefd[2];
	if (pipe(pipefd) != 0)
		return -1;
	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);
		execl("/bin/findfs", "findfs", spec, (char *)NULL);
		_exit(127);
	}

	close(pipefd[1]);
	ssize_t n = read(pipefd[0], out, outlen - 1);
	close(pipefd[0]);
	int status;
	waitpid(pid, &status, 0);
	if (n <= 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	out[n] = '\0';
	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' ||
	                 out[n - 1] == ' ' || out[n - 1] == '\t'))
		out[--n] = '\0';
	return strncmp(out, "/dev/", 5) == 0 ? 0 : -1;
}

/* Try to boot the real root: wait for the device to show up, mount it at
 * /mnt/root, pivot into it, and exec /sbin/init from there. */
static int boot_real_root(const char *root_spec)
{
	mkdir("/mnt", 0755);
	mkdir("/mnt/root", 0755);

	char root[256] = "";
	printf("GHL: real root %s requested, waiting for it...\n", root_spec);
	for (int i = 0; i < 50; i++) {
		if (resolve_root(root_spec, root, sizeof(root)) == 0 &&
		    access(root, F_OK) == 0)
			break;
		if (i % 10 == 0)
			printf("GHL: (waiting for %s, iteration %d)\n", root_spec, i);
		usleep(100000);
	}
	if (root[0] == '\0' || access(root, F_OK) != 0) {
		fprintf(stderr, "GHL: root device %s never appeared, staying in initramfs\n",
		        root_spec);
		return -1;
	}
	printf("GHL: resolved %s to %s\n", root_spec, root);

	if (mount(root, "/mnt/root", "ext4", 0, NULL) != 0) {
		perror("GHL: mount real root");
		return -1;
	}
	if (access("/mnt/root/sbin/init", F_OK) != 0) {
		fprintf(stderr, "GHL: no /sbin/init on the real root\n");
		return -1;
	}

	/* Move the real root onto / (the kernel does the same in
	 * prepare_namespace), then hand off to /sbin/init. */
	chdir("/mnt/root");
	if (mount(".", "/", NULL, MS_MOVE, NULL) != 0) {
		perror("GHL: MS_MOVE real root");
		return -1;
	}
	chroot(".");
	chdir("/");

	printf("GHL: moved to the real root\n");
	char *const argv[] = { "/sbin/init", NULL };
	char *const envp[] = { "HOME=/", "TERM=linux", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", NULL };
	execve("/sbin/init", argv, envp);
	perror("GHL: exec /sbin/init");
	return -1;
}

/* Run a one-shot script (e.g. /etc/initrc) and wait for it. */
static void run_once(const char *script)
{
	if (access(script, F_OK) != 0)
		return;
	printf("GHL: running %s\n", script);
	pid_t pid = fork();
	if (pid < 0) {
		perror("GHL: fork");
		return;
	}
	if (pid == 0) {
		char *const argv[] = { "/bin/sh", (char *)script, NULL };
		char *const envp[] = {
			"HOME=/root",
			"TERM=linux",
			"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
			NULL,
		};
		execve("/bin/sh", argv, envp);
		perror("GHL: exec /bin/sh");
		_exit(127);
	}
	int status;
	waitpid(pid, &status, 0);
}

/* Spawn the interactive shell. Returns its pid, or -1 if none. */
static pid_t spawn_shell(void)
{
	printf("GHL: handing off to a shell (ctrl-d to power off)\n");
	pid_t pid = fork();
	if (pid < 0) {
		perror("GHL: fork");
		return -1;
	}
	if (pid == 0) {
		/* PID 1 has no controlling terminal. Start a real session and attach
		 * the serial console so ash job control (ctrl-z, fg, bg) works. */
		setsid();
		int fd = open("/dev/ttyS0", O_RDWR);
		if (fd < 0)
			fd = open("/dev/console", O_RDWR);
		if (fd >= 0) {
			ioctl(fd, TIOCSCTTY, 1);
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);
			if (fd > 2)
				close(fd);
		}
		/* Login shell so /etc/profile runs (sets PATH + AMPKG_REPO). The
		 * display console gets the same root login environment. */
		char *const argv[] = { "/bin/sh", "-l", NULL };
		char *const envp[] = {
			"HOME=/root",
			"TERM=linux",
			"PATH=/bin:/sbin:/usr/bin:/usr/sbin",
			NULL,
		};
		execve("/bin/sh", argv, envp);
		perror("GHL: exec /bin/sh");
		_exit(127);
	}
	return pid;
}

/* Spawn a session on the display console (/dev/tty1, the framebuffer
 * console in the QEMU window). It's a login shell so /etc/profile runs and
 * opens directly into the GHL root environment. No-op when the display
 * console doesn't exist (headless). */
static pid_t spawn_display_session(void)
{
	int fd = open("/dev/tty1", O_RDWR);
	if (fd < 0)
		return -1;

	printf("GHL: root terminal on /dev/tty1\n");
	pid_t pid = fork();
	if (pid < 0) {
		close(fd);
		return -1;
	}
	if (pid == 0) {
		setsid();
		ioctl(fd, TIOCSCTTY, 1);
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
		setenv("GHL_DISPLAY", "1", 1);
		setenv("TERM", "linux", 1);
		setenv("HOME", "/root", 1);
		setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
		char *const argv[] = { "/bin/sh", "-l", NULL };
		execv("/bin/sh", argv);
		perror("GHL: exec display shell");
		_exit(127);
	}
	return pid;
}

static void power_off(void)
{
	printf("GHL: shell exited, powering off.\n");
	sync();
	reboot(RB_POWER_OFF);
	/* If the reboot call didn't take (no ACPI/APM), fall back to hanging. */
	sleep(60);
	for (;;)
		sleep(60);
}

int main(void)
{
	sethostname("ghl", 3);

	printf("\n");
	printf("  GHL \x1b[1;36mGuitar Hero Linux\x1b[0m — the guitar is the PC.\n");
	printf("  kernel booted, init running as pid 1.\n\n");

	setup_filesystems();
	/* Keep fbcon's native black background. Direct /dev/fb0 drawing survives
	 * behind text cells and makes the terminal look overlapped. */

	/* Bootstrapping role: if the kernel asked for a real root, try to
	 * hand off to it. The real root's /sbin/init (this same binary) then
	 * runs again with the whole userspace in place. */
	char root[128];
	if (cmdline_value("root", root, sizeof(root)) == 0 && access("/sbin/init", F_OK) != 0) {
		boot_real_root(root);
	}

	/* Supervisor role (runs as /sbin/init on the real root, or as the
	 * initramfs fallback): run the one-shot setup, then the consoles. */
	run_once("/etc/initrc");

	/* The serial console shell is the always-present one; the display
	 * console (tty1) appears when there's a framebuffer and opens a root
	 * terminal. The box powers off when the last one of
	 * them exits. */
	pid_t pids[2];
	int npids = 0;
	pid_t serial = spawn_shell();
	if (serial > 0)
		pids[npids++] = serial;
	pid_t display = spawn_display_session();
	if (display > 0)
		pids[npids++] = display;

	if (npids == 0) {
		printf("GHL: no shells, idling as pid 1.\n");
		for (;;)
			sleep(60);
	}

	/* Reap children forever. When every interactive console has exited,
	 * power off. Other children (e.g. udhcpc) are reaped but don't count. */
	for (;;) {
		int status;
		pid_t pid = waitpid(-1, &status, 0);
		if (pid <= 0)
			continue;
		int alive = 0;
		for (int i = 0; i < npids; i++) {
			if (pids[i] == pid)
				pids[i] = -1;
			if (pids[i] > 0)
				alive = 1;
		}
		if (!alive) {
			power_off();
			break;
		}
	}
}
