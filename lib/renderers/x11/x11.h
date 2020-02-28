#ifndef _BM_X11_H_
#define _BM_X11_H_

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "renderers/cairo.h"

enum mod_bit {
    MOD_SHIFT = 1<<0,
    MOD_CTRL = 1<<1,
    MOD_ALT = 1<<2,
};

struct buffer {
    struct cairo cairo;
    uint32_t width, height;
    bool created;
};

struct window {
    Display *display;
    int32_t screen;
    Drawable drawable;
    XIM xim;
    XIC xic;
    Visual *visual;

    KeySym keysym;
    uint32_t mods;

    struct buffer buffer;
    uint32_t x, y, width, height, max_height;
    uint32_t displayed;

    uint32_t monitor;
    bool bottom;

    struct {
        void (*render)(struct cairo *cairo, uint32_t width, uint32_t max_height, const struct bm_menu *menu, struct cairo_paint_result *result);
    } notify;
};

struct x11 {
    Display *display;
    struct window window;
};

void bm_x11_window_render(struct window *window, const struct bm_menu *menu);
void bm_x11_window_key_press(struct window *window, XKeyEvent *ev);
void bm_x11_window_set_monitor(struct window *window, uint32_t monitor);
void bm_x11_window_set_bottom(struct window *window, bool bottom);
bool bm_x11_window_create(struct window *window, Display *display);
void bm_x11_window_destroy(struct window *window);

#endif /* _BM_WAYLAND_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
