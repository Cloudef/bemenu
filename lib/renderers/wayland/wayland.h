#ifndef _BM_WAYLAND_H_
#define _BM_WAYLAND_H_

#include "internal.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>

#include "wlr-layer-shell-unstable-v1.h"
#include "fractional-scale-v1.h"
#include "viewporter.h"
#include "renderers/cairo_renderer.h"

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

extern const char *BM_XKB_MASK_NAMES[MASK_LAST];
extern const enum mod_bit BM_XKB_MODS[MASK_LAST];

struct xkb {
    struct xkb_state *state;
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    xkb_mod_mask_t masks[MASK_LAST];
};

struct pointer_event {
    uint32_t event_mask;
    wl_fixed_t surface_x, surface_y;
    uint32_t button, state;
    uint32_t time;
    uint32_t serial;
    struct {
        bool valid;
        wl_fixed_t value;
        int32_t discrete;
    } axes[2];
    uint32_t axis_source;
};

struct touch_point {
    bool valid;
    int32_t id;
    uint32_t event_mask;
    wl_fixed_t surface_x, surface_y;
    wl_fixed_t surface_start_x, surface_start_y;
    wl_fixed_t major, minor;
    wl_fixed_t orientation;
};

struct touch_event {
    uint32_t time;
    uint32_t serial;
    uint16_t active;
    struct touch_point points[2];
};

struct input {
    int *repeat_fd;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_touch *touch;
    struct pointer_event pointer_event;
    struct touch_event touch_event;
    struct xkb xkb;

    xkb_keysym_t sym;
    uint32_t code;
    uint32_t modifiers;

    xkb_keysym_t repeat_sym;
    uint32_t repeat_key;

    int32_t repeat_rate_sec;
    int32_t repeat_rate_nsec;
    int32_t repeat_delay_sec;
    int32_t repeat_delay_nsec;

    struct {
        void (*key)(enum wl_keyboard_key_state state, xkb_keysym_t sym, uint32_t code);
    } notify;

    bool key_pending;
};

struct buffer {
    struct cairo cairo;
    struct wl_buffer *buffer;
    uint32_t width, height;
    bool busy;
};

struct window {
    struct wayland *wayland;
    struct wl_list surf_outputs;
    struct wl_surface *surface;
    struct wl_callback *frame_cb;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wp_viewport *viewport_surface;
    struct wl_shm *shm;
    struct buffer buffers[2];
    uint32_t width, height, max_height;
    uint32_t hmargin_size;
    float width_factor;
    double scale;
    uint32_t displayed;
    struct wl_list link;
    enum bm_align align;
    int32_t y_offset;
    uint32_t align_anchor;
    bool render_pending;

    struct {
        void (*render)(struct cairo *cairo, uint32_t width, uint32_t max_height, struct bm_menu *menu, struct cairo_paint_result *result);
    } notify;
};

struct output {
    struct wl_output *output;
    struct wl_list link;
    uint32_t height;
    int scale;
    char *name;
};

struct surf_output {
    struct output *output;
    struct wl_list link;
};

struct wayland {
    struct {
        int32_t display;
        int32_t repeat;
    } fds;

    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_list outputs;
    struct output *selected_output;
    struct wl_seat *seat;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_shm *shm;
    struct input input;
    struct wl_list windows;
    uint32_t formats;
    struct wp_fractional_scale_manager_v1 *wfs_mgr;
    struct wp_viewporter *viewporter;
    bool fractional_scaling;
};

void bm_wl_repeat(struct wayland *wayland);
bool bm_wl_registry_register(struct wayland *wayland);
void bm_wl_registry_destroy(struct wayland *wayland);
void bm_wl_window_schedule_render(struct window *window);
void bm_wl_window_render(struct window *window, struct wl_display *display, struct bm_menu *menu);
void bm_wl_window_set_width(struct window *window, struct wl_display *display, uint32_t margin, float factor);
void bm_wl_window_set_align(struct window *window, struct wl_display *display, enum bm_align align);
void bm_wl_window_set_y_offset(struct window *window, struct wl_display *display, int32_t y_offset);
void bm_wl_window_grab_keyboard(struct window *window, struct wl_display *display, bool grab);
void bm_wl_window_set_overlap(struct window *window, struct wl_display *display, bool overlap);
bool bm_wl_window_create(struct window *window, struct wl_display *display, struct wl_shm *shm, struct wl_output *output, struct zwlr_layer_shell_v1 *layer_shell, struct wl_surface *surface);
void bm_wl_window_destroy(struct window *window);

void recreate_windows(const struct bm_menu *menu, struct wayland *wayland);
void window_update_output(struct window *window);

#endif /* _BM_WAYLAND_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
