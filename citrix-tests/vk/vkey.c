/* press SUPER+T via zwp_virtual_keyboard_v1 */
#define _GNU_SOURCE
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include "vk.h"
static struct wl_seat* seat; static struct zwp_virtual_keyboard_manager_v1* mgr;
static void g(void* d, struct wl_registry* r, uint32_t n, const char* i, uint32_t v) {
    if (!strcmp(i, wl_seat_interface.name)) seat = wl_registry_bind(r, n, &wl_seat_interface, 1);
    if (!strcmp(i, zwp_virtual_keyboard_manager_v1_interface.name)) mgr = wl_registry_bind(r, n, &zwp_virtual_keyboard_manager_v1_interface, 1);
}
static void gr(void* d, struct wl_registry* r, uint32_t n) {}
static const struct wl_registry_listener rl = {g, gr};
static uint32_t ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }
int main(void) {
    struct wl_display* dpy = wl_display_connect(NULL); if (!dpy) return 1;
    struct wl_registry* reg = wl_display_get_registry(dpy); wl_registry_add_listener(reg, &rl, NULL); wl_display_roundtrip(dpy);
    if (!seat || !mgr) { fprintf(stderr, "no vk\n"); return 1; }
    struct zwp_virtual_keyboard_v1* kb = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(mgr, seat);
    struct xkb_context* ctx = xkb_context_new(0);
    struct xkb_rule_names names = {0};
    struct xkb_keymap* km = xkb_keymap_new_from_names(ctx, &names, 0);
    char* s = xkb_keymap_get_as_string(km, XKB_KEYMAP_FORMAT_TEXT_V1); size_t len = strlen(s) + 1;
    int fd = memfd_create("km", 0); write(fd, s, len);
    zwp_virtual_keyboard_v1_keymap(kb, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, len);
    wl_display_roundtrip(dpy);
    const uint32_t SUPER = 125, T = 20; /* evdev codes: KEY_LEFTMETA, KEY_T */
    zwp_virtual_keyboard_v1_key(kb, ms(), SUPER, 1); zwp_virtual_keyboard_v1_modifiers(kb, 64, 0, 0, 0); wl_display_roundtrip(dpy); usleep(30000);
    zwp_virtual_keyboard_v1_key(kb, ms(), T, 1); wl_display_roundtrip(dpy); usleep(30000);
    zwp_virtual_keyboard_v1_key(kb, ms(), T, 0); wl_display_roundtrip(dpy); usleep(30000);
    zwp_virtual_keyboard_v1_key(kb, ms(), SUPER, 0); zwp_virtual_keyboard_v1_modifiers(kb, 0, 0, 0, 0); wl_display_roundtrip(dpy);
    usleep(100000);
    return 0;
}
