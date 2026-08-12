/* GHL Desktop Mode backdrop and status source.
 *
 * Deliberately plain Xlib: the desktop should look like a workbench attached
 * to the game system, not a miniature general-purpose distro. */
#include <X11/Xlib.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

static unsigned long color(Display *dpy, int screen, const char *hex) {
    XColor value, exact;
    if (!XAllocNamedColor(dpy, DefaultColormap(dpy, screen), hex, &value, &exact))
        return BlackPixel(dpy, screen);
    return value.pixel;
}

static void line(Display *dpy, Drawable out, GC gc, unsigned long pixel,
                 int x1, int y1, int x2, int y2, int width) {
    XSetForeground(dpy, gc, pixel);
    XSetLineAttributes(dpy, gc, (unsigned int)width, LineSolid, CapButt, JoinMiter);
    XDrawLine(dpy, out, gc, x1, y1, x2, y2);
}

static void text(Display *dpy, Drawable out, GC gc, unsigned long pixel,
                 int x, int y, const char *value) {
    XSetForeground(dpy, gc, pixel);
    XDrawString(dpy, out, gc, x, y, value, (int)strlen(value));
}

static void address(char *out, size_t length) {
    snprintf(out, length, "offline");
    struct ifaddrs *addresses = NULL;
    if (getifaddrs(&addresses) != 0) return;
    for (struct ifaddrs *it = addresses; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET ||
            !strcmp(it->ifa_name, "lo")) continue;
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)it->ifa_addr;
        if (inet_ntop(AF_INET, &ipv4->sin_addr, out, (socklen_t)length)) break;
    }
    freeifaddrs(addresses);
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "ghl-bg: cannot open display\n");
        return 1;
    }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int w = DisplayWidth(dpy, screen), h = DisplayHeight(dpy, screen);
    unsigned long ink = color(dpy, screen, "#09090d");
    unsigned long surface = color(dpy, screen, "#17181d");
    unsigned long rule = color(dpy, screen, "#35363b");
    unsigned long paper = color(dpy, screen, "#f4f1e0");
    unsigned long muted = color(dpy, screen, "#77776f");
    unsigned long acid = color(dpy, screen, "#d7ff3a");

    Pixmap canvas = XCreatePixmap(dpy, root, (unsigned int)w, (unsigned int)h,
                                  (unsigned int)DefaultDepth(dpy, screen));
    GC gc = XCreateGC(dpy, canvas, 0, NULL);
    XSetForeground(dpy, gc, ink);
    XFillRectangle(dpy, canvas, gc, 0, 0, (unsigned int)w, (unsigned int)h);

    /* Left registration rail and an intentionally off-centre work area. */
    XSetForeground(dpy, gc, surface);
    XFillRectangle(dpy, canvas, gc, 42, 70, 248, (unsigned int)(h - 132));
    XSetForeground(dpy, gc, acid);
    XFillRectangle(dpy, canvas, gc, 42, 70, 7, (unsigned int)(h - 132));
    text(dpy, canvas, gc, paper, 72, 116, "GHL / DESKTOP MODE");
    text(dpy, canvas, gc, muted, 72, 144, "a workbench, not a second OS");
    line(dpy, canvas, gc, rule, 72, 174, 260, 174, 1);
    text(dpy, canvas, gc, acid, 72, 217, "ALT + P");
    text(dpy, canvas, gc, paper, 185, 217, "launcher");
    text(dpy, canvas, gc, acid, 72, 253, "ALT+SHIFT+ENTER");
    text(dpy, canvas, gc, paper, 185, 253, "terminal");
    text(dpy, canvas, gc, acid, 72, 289, "ALT + ESC");
    text(dpy, canvas, gc, paper, 185, 289, "Backstage");

    /* The large mark is built from rules, so it stays crisp without assets. */
    int mark_x = w / 2 - 80, mark_y = h / 2 - 110;
    line(dpy, canvas, gc, rule, mark_x, mark_y, mark_x + 330, mark_y, 3);
    line(dpy, canvas, gc, paper, mark_x, mark_y, mark_x, mark_y + 170, 14);
    line(dpy, canvas, gc, paper, mark_x, mark_y + 170, mark_x + 100, mark_y + 170, 14);
    line(dpy, canvas, gc, paper, mark_x + 100, mark_y + 170, mark_x + 100, mark_y + 95, 14);
    line(dpy, canvas, gc, paper, mark_x + 100, mark_y + 95, mark_x + 52, mark_y + 95, 14);
    line(dpy, canvas, gc, acid, mark_x + 142, mark_y, mark_x + 142, mark_y + 170, 14);
    line(dpy, canvas, gc, acid, mark_x + 142, mark_y + 78, mark_x + 224, mark_y + 78, 14);
    line(dpy, canvas, gc, acid, mark_x + 224, mark_y, mark_x + 224, mark_y + 170, 14);
    line(dpy, canvas, gc, paper, mark_x + 268, mark_y, mark_x + 268, mark_y + 170, 14);
    line(dpy, canvas, gc, paper, mark_x + 268, mark_y + 170, mark_x + 350, mark_y + 170, 14);
    text(dpy, canvas, gc, muted, mark_x, mark_y + 214, "FILES / TERMINAL / RECOVERY");

    line(dpy, canvas, gc, rule, 42, h - 40, w - 42, h - 40, 1);
    text(dpy, canvas, gc, muted, 42, h - 18, "THE GUITAR IS THE COMPUTER  /  ENCORE 0.1");

    XSetWindowBackgroundPixmap(dpy, root, canvas);
    XClearWindow(dpy, root);
    XFreePixmap(dpy, canvas);
    XFreeGC(dpy, gc);
    XFlush(dpy);

    for (;;) {
        time_t now = time(NULL);
        struct tm local;
        localtime_r(&now, &local);
        char clock[16], ip[32], status[160];
        strftime(clock, sizeof clock, "%H:%M", &local);
        address(ip, sizeof ip);
        struct sysinfo memory;
        unsigned long used = 0;
        if (sysinfo(&memory) == 0 && memory.totalram)
            used = (unsigned long)((memory.totalram - memory.freeram) * 100 / memory.totalram);
        snprintf(status, sizeof status, "GHL 0.1  |  net %s  |  ram %lu%%  |  %s", ip, used, clock);
        XStoreName(dpy, root, status);
        XFlush(dpy);
        sleep(2);
    }
}
