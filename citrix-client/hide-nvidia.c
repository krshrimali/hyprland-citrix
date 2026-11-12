/*
 * LD_PRELOAD shim for Citrix Workspace (wfica) on hybrid Intel+NVIDIA machines.
 *
 * wfica disables hardware H.264 decoding ("Hardware decoding disabled because Nvidia GPU is
 * installed"). Its GPU list (ctxeptlm::GetGpuVendors) comes from /sys/bus/pci/devices/<dev>/class
 * == "0x030000" (VGA) plus <dev>/vendor; a desktop Intel iGPU is "0x038000" and isn't counted.
 * This makes the /sys "class" file read 0x030200 (3D controller, like a laptop dGPU) for NVIDIA
 * and 0x030000 for Intel, and hides /sys/class/drm/cardN/device/{uevent,vendor} of NVIDIA cards.
 * wfica then sees one Intel GPU, keeps OpenGL (still rendered on NVIDIA via EGL; /dev/nvidia*,
 * PCI config space and libdrm's /sys/dev/char paths are untouched) and decodes via VA-API on Intel.
 * CITRIX_HIDE_NVIDIA_DEBUG=<file> logs every /sys open to that file.
 *
 * Disable with CITRIX_HIDE_NVIDIA=0. Removes itself from LD_PRELOAD so child processes don't inherit it.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>

static int enabled = 1;
static const char* debug; /* log path; reopened per line because wfica closes inherited fds */

__attribute__((constructor)) static void init(void) {
    const char* e = getenv("CITRIX_HIDE_NVIDIA");
    if (e && !strcmp(e, "0"))
        enabled = 0;
    const char* d = getenv("CITRIX_HIDE_NVIDIA_DEBUG");
    if (d && *d)
        debug = d;
    /* drop ourselves from LD_PRELOAD for children */
    const char* lp = getenv("LD_PRELOAD");
    if (lp && strstr(lp, "hide-nvidia")) {
        char buf[4096], out[4096] = "";
        snprintf(buf, sizeof buf, "%s", lp);
        for (char* tok = strtok(buf, ": "); tok; tok = strtok(NULL, ": ")) {
            if (strstr(tok, "hide-nvidia"))
                continue;
            if (*out)
                strncat(out, ":", sizeof out - strlen(out) - 1);
            strncat(out, tok, sizeof out - strlen(out) - 1);
        }
        if (*out)
            setenv("LD_PRELOAD", out, 1);
        else
            unsetenv("LD_PRELOAD");
    }
}

/* "/sys/class/drm/cardN/device/uevent" or ".../device/vendor" of an NVIDIA card? */
static int hidden(const char* path) {
    static const char pre[] = "/sys/class/drm/card";
    if (!enabled || !path || strncmp(path, pre, sizeof pre - 1))
        return 0;
    const char* p = path + sizeof pre - 1;
    if (*p < '0' || *p > '9')
        return 0;
    while (*p >= '0' && *p <= '9')
        p++;
    if (strcmp(p, "/device/uevent") && strcmp(p, "/device/vendor"))
        return 0;
    char vpath[256];
    snprintf(vpath, sizeof vpath, "%.*s/device/vendor", (int)(p - path), path);
    int fd = syscall(SYS_openat, AT_FDCWD, vpath, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    char v[16] = {0};
    ssize_t n = syscall(SYS_read, fd, v, sizeof v - 1);
    syscall(SYS_close, fd);
    if (n <= 0 || strncmp(v, "0x10de", 6))
        return 0;
    static int logged;
    if (!logged++)
        fprintf(stderr, "citrix-hide-nvidia: hiding NVIDIA DRM card from wfica GPU scan (%s)\n", path);
    return 1;
}

static int readsys(const char* path, char* out, size_t len) {
    int fd = syscall(SYS_openat, AT_FDCWD, path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    ssize_t n = syscall(SYS_read, fd, out, len - 1);
    syscall(SYS_close, fd);
    if (n < 0)
        return -1;
    out[n] = 0;
    return 0;
}

/* returns fake contents for /sys/bus/pci/devices/<dev>/class (or the /sys/devices/... form), else NULL */
static const char* faked(const char* path) {
    if (debug && path) {
        char line[600];
        int  n  = snprintf(line, sizeof line, "open %s\n", path);
        int  fd = syscall(SYS_openat, AT_FDCWD, debug, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
        if (fd >= 0) {
            if (syscall(SYS_write, fd, line, n > 0 && n < (int)sizeof line ? n : (int)sizeof line - 1) < 0) {}
            syscall(SYS_close, fd);
        }
    }
    if (!enabled || !path || strncmp(path, "/sys/", 5))
        return NULL;
    size_t l = strlen(path);
    if (l < 7 || strcmp(path + l - 6, "/class") || (!strstr(path, "/pci") && strncmp(path, "/sys/bus/pci/devices/", 21)))
        return NULL;
    char vpath[512], v[32], c[32];
    snprintf(vpath, sizeof vpath, "%.*s/vendor", (int)(l - 6), path);
    if (readsys(vpath, v, sizeof v) || readsys(path, c, sizeof c))
        return NULL;
    if (!strncmp(v, "0x10de", 6) && !strncmp(c, "0x0300", 6))
        return "0x030200\n";
    if (!strncmp(v, "0x8086", 6) && !strncmp(c, "0x0380", 6))
        return "0x030000\n";
    return NULL;
}

static int fakefd(const char* content) {
    static int logged;
    if (!logged++)
        fprintf(stderr, "citrix-hide-nvidia: presenting Intel iGPU as VGA and NVIDIA as 3D controller to wfica\n");
    int fd = memfd_create("citrix-sysfs", MFD_CLOEXEC);
    if (fd < 0)
        return -1;
    write(fd, content, strlen(content));
    lseek(fd, 0, SEEK_SET);
    return fd;
}

#define REAL(name) static __typeof__(name)* real_##name; if (!real_##name) real_##name = dlsym(RTLD_NEXT, #name)

FILE* fopen(const char* p, const char* m) { REAL(fopen); if (hidden(p)) { errno = ENOENT; return NULL; } const char* f = faked(p); if (f) { int fd = fakefd(f); if (fd >= 0) return fdopen(fd, "r"); } return real_fopen(p, m); }
FILE* fopen64(const char* p, const char* m) { REAL(fopen64); if (hidden(p)) { errno = ENOENT; return NULL; } const char* f = faked(p); if (f) { int fd = fakefd(f); if (fd >= 0) return fdopen(fd, "r"); } return real_fopen64(p, m); }

#define OPEN_MODE(flags) mode_t mode = 0; if ((flags) & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
int open(const char* p, int f, ...) { REAL(open); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } return real_open(p, f, mode); }
int open64(const char* p, int f, ...) { REAL(open64); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } return real_open64(p, f, mode); }
int openat(int d, const char* p, int f, ...) { REAL(openat); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } return real_openat(d, p, f, mode); }
int openat64(int d, const char* p, int f, ...) { REAL(openat64); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } return real_openat64(d, p, f, mode); }
int __open_2(const char* p, int f) { if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } REAL(__open_2); return real___open_2(p, f); }
int __open64_2(const char* p, int f) { if (hidden(p)) { errno = ENOENT; return -1; } { const char* fk = faked(p); if (fk) { int fd = fakefd(fk); if (fd >= 0) return fd; } } REAL(__open64_2); return real___open64_2(p, f); }
