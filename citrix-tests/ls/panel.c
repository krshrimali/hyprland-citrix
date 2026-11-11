/* top-layer panel test client: solid blue 300px bar anchored to the top of the output named argv[1] */
#define _GNU_SOURCE
#include <wayland-client.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ls.h"

static struct wl_compositor*       comp;
static struct wl_shm*              shm;
static struct zwlr_layer_shell_v1* lsh;
static struct wl_output*           outs[8];
static int                         nouts;
static struct wl_output*           target;
static const char*                 want;

static void oname(void* d, struct wl_output* o, const char* n) {
    if (!strcmp(n, want))
        target = o;
}
static void ogeom(void* d, struct wl_output* o, int32_t a, int32_t b, int32_t c, int32_t e, int32_t f, const char* g, const char* h, int32_t i) {}
static void omode(void* d, struct wl_output* o, uint32_t a, int32_t b, int32_t c, int32_t e) {}
static void odone(void* d, struct wl_output* o) {}
static void oscale(void* d, struct wl_output* o, int32_t s) {}
static void odesc(void* d, struct wl_output* o, const char* s) {}
static const struct wl_output_listener ol = {ogeom, omode, odone, oscale, oname, odesc};

static void g(void* d, struct wl_registry* r, uint32_t n, const char* i, uint32_t v) {
    if (!strcmp(i, "wl_compositor"))
        comp = wl_registry_bind(r, n, &wl_compositor_interface, 4);
    else if (!strcmp(i, "wl_shm"))
        shm = wl_registry_bind(r, n, &wl_shm_interface, 1);
    else if (!strcmp(i, "zwlr_layer_shell_v1"))
        lsh = wl_registry_bind(r, n, &zwlr_layer_shell_v1_interface, 1);
    else if (!strcmp(i, "wl_output") && nouts < 8) {
        outs[nouts] = wl_registry_bind(r, n, &wl_output_interface, 4);
        wl_output_add_listener(outs[nouts++], &ol, NULL);
    }
}
static void gr(void* d, struct wl_registry* r, uint32_t n) {}
static const struct wl_registry_listener rl = {g, gr};

static uint32_t W, H;
static int      configured;
static void     lconf(void* d, struct zwlr_layer_surface_v1* s, uint32_t serial, uint32_t w, uint32_t h) {
    W          = w;
    H          = h;
    configured = 1;
    zwlr_layer_surface_v1_ack_configure(s, serial);
}
static void lclose(void* d, struct zwlr_layer_surface_v1* s) {}
static const struct zwlr_layer_surface_v1_listener ll = {lconf, lclose};

int main(int argc, char** argv) {
    want                     = argv[1];
    struct wl_display*  dpy  = wl_display_connect(NULL);
    struct wl_registry* reg  = wl_display_get_registry(dpy);
    wl_registry_add_listener(reg, &rl, NULL);
    wl_display_roundtrip(dpy);
    wl_display_roundtrip(dpy);
    if (!target) {
        fprintf(stderr, "no output %s\n", want);
        return 1;
    }
    struct wl_surface*            surf = wl_compositor_create_surface(comp);
    struct zwlr_layer_surface_v1* ls   = zwlr_layer_shell_v1_get_layer_surface(lsh, surf, target, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "testpanel");
    zwlr_layer_surface_v1_set_anchor(ls, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(ls, 0, 300);
    zwlr_layer_surface_v1_set_exclusive_zone(ls, 300);
    zwlr_layer_surface_v1_add_listener(ls, &ll, NULL);
    wl_surface_commit(surf);
    while (!configured)
        wl_display_dispatch(dpy);

    int stride = W * 4, size = stride * H;
    int fd     = memfd_create("buf", 0);
    ftruncate(fd, size);
    uint32_t* px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    for (int i = 0; i < size / 4; i++)
        px[i] = 0xff0000ff;
    struct wl_shm_pool* pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer*   buf  = wl_shm_pool_create_buffer(pool, 0, W, H, stride, WL_SHM_FORMAT_ARGB8888);
    wl_surface_attach(surf, buf, 0, 0);
    wl_surface_damage(surf, 0, 0, W, H);
    wl_surface_commit(surf);
    while (wl_display_dispatch(dpy) != -1) {
    }
}
