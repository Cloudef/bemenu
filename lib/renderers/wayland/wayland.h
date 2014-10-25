#ifndef _BM_WAYLAND_H_
#define _BM_WAYLAND_H_

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "wayland-xdg-shell-client-protocol.h"

#include "renderers/cairo.h"

struct bm_menu;

enum mod_bit {
    MOD_SHIFT = 1<<0,
    MOD_CAPS = 1<<1,
    MOD_CTRL = 1<<2,
    MOD_ALT = 1<<3,
    MOD_MOD2 = 1<<4,
    MOD_MOD3 = 1<<5,
    MOD_LOGO = 1<<6,
    MOD_MOD5 = 1<<7,
};

enum mask {
    MASK_SHIFT,
    MASK_CAPS,
    MASK_CTRL,
    MASK_ALT,
    MASK_MOD2,
    MASK_MOD3,
    MASK_LOGO,
    MASK_MOD5,
    MASK_LAST
};

const char *BM_XKB_MASK_NAMES[MASK_LAST];
const enum mod_bit BM_XKB_MODS[MASK_LAST];

struct xkb {
    struct xkb_state *state;
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    xkb_mod_mask_t masks[MASK_LAST];
};

struct input {
    struct wl_keyboard *keyboard;
    struct xkb xkb;

    xkb_keysym_t sym;
    uint32_t code;
    uint32_t modifiers;

    struct {
        void (*key)(enum wl_keyboard_key_state state, xkb_keysym_t sym, uint32_t code);
    } notify;
};

struct buffer {
    struct cairo cairo;
    struct wl_buffer *buffer;
    uint32_t width, height;
    bool busy;
};

struct window {
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct xdg_surface *xdg_surface;
    struct wl_shm *shm;
    struct buffer buffers[2];
    uint32_t width, height;
    uint32_t displayed;

    struct {
        uint32_t (*render)(struct cairo *cairo, uint32_t width, uint32_t height, const struct bm_menu *menu);
    } notify;
};

struct wayland {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_seat *seat;
    struct xdg_shell *xdg_shell;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct input input;
    struct window window;
    uint32_t formats;
};

bool bm_wl_registry_register(struct wayland *wayland);
void bm_wl_registry_destroy(struct wayland *wayland);
void bm_wl_window_render(struct window *window, const struct bm_menu *menu);
bool bm_wl_window_create(struct window *window, struct wl_shm *shm, struct wl_shell *shell, struct xdg_shell *xdg_shell, struct wl_surface *surface);
void bm_wl_window_destroy(struct window *window);

#endif /* _BM_WAYLAND_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
