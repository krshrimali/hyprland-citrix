/*
 * LD_PRELOAD shim for Citrix Workspace (wfica) on hybrid Intel+NVIDIA machines.
 *
 * wfica disables hardware H.264 decoding ("Hardware decoding disabled because Nvidia GPU is
 * installed") after scanning /sys/class/drm/cardN/device/{uevent,vendor}. This hides exactly
 * those two files for NVIDIA cards (vendor 0x10de), so wfica only sees the Intel iGPU and decodes
 * via VA-API on it. Nothing else is touched: NVIDIA EGL/OpenGL, libdrm (/sys/dev/char/...) and
 * /dev/nvidia* stay visible, so OpenGL rendering keeps running on the NVIDIA card.
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

static int enabled = 1;

__attribute__((constructor)) static void init(void) {
    const char* e = getenv("CITRIX_HIDE_NVIDIA");
    if (e && !strcmp(e, "0"))
        enabled = 0;
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

#define REAL(name) static __typeof__(name)* real_##name; if (!real_##name) real_##name = dlsym(RTLD_NEXT, #name)

FILE* fopen(const char* p, const char* m) { REAL(fopen); if (hidden(p)) { errno = ENOENT; return NULL; } return real_fopen(p, m); }
FILE* fopen64(const char* p, const char* m) { REAL(fopen64); if (hidden(p)) { errno = ENOENT; return NULL; } return real_fopen64(p, m); }

#define OPEN_MODE(flags) mode_t mode = 0; if ((flags) & (O_CREAT | O_TMPFILE)) { va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
int open(const char* p, int f, ...) { REAL(open); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } return real_open(p, f, mode); }
int open64(const char* p, int f, ...) { REAL(open64); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } return real_open64(p, f, mode); }
int openat(int d, const char* p, int f, ...) { REAL(openat); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } return real_openat(d, p, f, mode); }
int openat64(int d, const char* p, int f, ...) { REAL(openat64); OPEN_MODE(f); if (hidden(p)) { errno = ENOENT; return -1; } return real_openat64(d, p, f, mode); }
int __open_2(const char* p, int f) { if (hidden(p)) { errno = ENOENT; return -1; } REAL(__open_2); return real___open_2(p, f); }
int __open64_2(const char* p, int f) { if (hidden(p)) { errno = ENOENT; return -1; } REAL(__open64_2); return real___open64_2(p, f); }
