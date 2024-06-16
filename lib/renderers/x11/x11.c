#include "internal.h"
#include "x11.h"
#include "xkb_unicode.h"

#include <stdlib.h>
#include <unistd.h>
#include <X11/Xutil.h>

static bool
render(struct bm_menu *menu)
{
    struct x11 *x11 = menu->renderer->internal;

    bm_x11_window_render(&x11->window, menu);
    XFlush(x11->display);

    XEvent ev;
    if (XNextEvent(x11->display, &ev) || XFilterEvent(&ev, x11->window.drawable))
        return true;

    switch (ev.type) {
        case KeyPress:
            bm_x11_window_key_press(&x11->window, &ev.xkey);
            break;
        case SelectionNotify:
            // paste here
            break;
        case VisibilityNotify:
            if (ev.xvisibility.state != VisibilityUnobscured) {
                XRaiseWindow(x11->display, x11->window.drawable);
                XFlush(x11->display);
            }
            break;
    }

    return true;
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11 && unicode);

    if (x11->window.keysym == NoSymbol)
        return BM_KEY_UNICODE;

    KeySym sym = x11->window.keysym;
    uint32_t mods = x11->window.mods;
    *unicode = bm_x11_key_sym2unicode(sym);

    x11->window.keysym = NoSymbol;

    switch (sym) {
        case XK_Up:
            return BM_KEY_UP;

        case XK_Down:
            return BM_KEY_DOWN;

        case XK_Left:
            return (mods & MOD_SHIFT ? BM_KEY_UP : BM_KEY_LEFT);

        case XK_Right:
            return (mods & MOD_SHIFT ? BM_KEY_DOWN : BM_KEY_RIGHT);

        case XK_Home:
            return BM_KEY_HOME;

        case XK_End:
            return BM_KEY_END;

        case XK_Page_Up:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_UP : BM_KEY_PAGE_UP);

        case XK_less:
            return (mods & MOD_ALT ? BM_KEY_SHIFT_PAGE_UP : BM_KEY_UNICODE);

        case XK_Page_Down:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_DOWN : BM_KEY_PAGE_DOWN);

        case XK_greater:
            return (mods & MOD_ALT ? BM_KEY_SHIFT_PAGE_DOWN : BM_KEY_UNICODE);

        case XK_v:
            return (mods & MOD_CTRL ? BM_KEY_PAGE_DOWN : (mods & MOD_ALT ? BM_KEY_PAGE_UP : BM_KEY_UNICODE));

        case XK_BackSpace:
            return BM_KEY_BACKSPACE;

        case XK_Delete:
            return (mods & MOD_SHIFT ? BM_KEY_LINE_DELETE_LEFT : BM_KEY_DELETE);

        case XK_Tab:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_TAB : BM_KEY_TAB);

        case XK_ISO_Left_Tab:
            return BM_KEY_SHIFT_TAB;

        case XK_Insert:
            return BM_KEY_SHIFT_RETURN;

        case XK_KP_Enter:
        case XK_Return:
            return (mods & MOD_CTRL ? BM_KEY_CONTROL_RETURN : (mods & MOD_SHIFT ? BM_KEY_SHIFT_RETURN : BM_KEY_RETURN));

        case XK_g:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XK_c:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XK_bracketleft:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XK_Escape:
            return BM_KEY_ESCAPE;

        case XK_p:
            return (mods & MOD_CTRL ? BM_KEY_UP : BM_KEY_UNICODE);

        case XK_n:
            return (mods & MOD_CTRL ? BM_KEY_DOWN : BM_KEY_UNICODE);

        case XK_l:
            return (mods & MOD_CTRL ? BM_KEY_LEFT : (mods & MOD_ALT ? BM_KEY_DOWN : BM_KEY_UNICODE));

        case XK_b:
            return (mods & MOD_CTRL ? BM_KEY_LEFT : BM_KEY_UNICODE);

        case XK_f:
            return (mods & MOD_CTRL ? BM_KEY_RIGHT : BM_KEY_UNICODE);

        case XK_a:
            return (mods & MOD_CTRL ? BM_KEY_HOME : BM_KEY_UNICODE);

        case XK_e:
            return (mods & MOD_CTRL ? BM_KEY_END : BM_KEY_UNICODE);

        case XK_h:
            return (mods & MOD_CTRL ? BM_KEY_BACKSPACE : (mods & MOD_ALT ? BM_KEY_UP : BM_KEY_UNICODE));

        case XK_u:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_LEFT : (mods & MOD_ALT ? BM_KEY_PAGE_UP : BM_KEY_UNICODE));

        case XK_k:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_RIGHT : (mods & MOD_ALT ? BM_KEY_UP : BM_KEY_UNICODE));

        case XK_w:
            return (mods & MOD_CTRL ? BM_KEY_WORD_DELETE : BM_KEY_UNICODE);

        case XK_y:
            return (mods & MOD_CTRL ? BM_KEY_PASTE_CLIPBOARD : BM_KEY_UNICODE);

        case XK_Y:
            return (mods & MOD_CTRL ? BM_KEY_PASTE_PRIMARY : BM_KEY_UNICODE);

        case XK_j:
            return (mods & MOD_ALT ? BM_KEY_DOWN : BM_KEY_UNICODE);

        case XK_d:
            return (mods & MOD_ALT ? BM_KEY_PAGE_DOWN : BM_KEY_UNICODE);

        case XK_m:
            return (mods & MOD_CTRL ? BM_KEY_RETURN : BM_KEY_UNICODE);

        case XK_1:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_1;
            break;
        case XK_2:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_2;
            break;
        case XK_3:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_3;
            break;
        case XK_4:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_4;
            break;
        case XK_5:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_5;
            break;
        case XK_6:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_6;
            break;
        case XK_7:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_7;
            break;
        case XK_8:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_8;
            break;
        case XK_9:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_9;
            break;
        case XK_0:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_10;
            break;

        default: break;
    }

    return BM_KEY_UNICODE;
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);
    return x11->window.displayed;
}

static void
set_align(const struct bm_menu *menu, enum bm_align align)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);
    bm_x11_window_set_align(&x11->window, align);
}

static void
set_y_offset(const struct bm_menu *menu, int32_t y_offset)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);
    bm_x11_window_set_y_offset(&x11->window, y_offset);
}

static void
set_width(const struct bm_menu *menu, uint32_t margin, float factor)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);
    bm_x11_window_set_width(&x11->window, margin, factor);
}

static void
set_monitor(const struct bm_menu *menu, int32_t monitor)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);
    bm_x11_window_set_monitor(&x11->window, monitor);
}

static void
grab_keyboard(const struct bm_menu *menu, bool grab)
{
    struct x11 *x11 = menu->renderer->internal;
    assert(x11);

    if (grab) {
        for (uint32_t i = 0; i < 1000; ++i) {
            if (XGrabKeyboard(x11->display, DefaultRootWindow(x11->display), True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
                return;
            usleep(1000);
        }

        fprintf(stderr, "x11: cannot grab keyboard\n");
    } else {
        XUngrabKeyboard(x11->display, CurrentTime);
    }
}

static void
destructor(struct bm_menu *menu)
{
    struct x11 *x11 = menu->renderer->internal;

    if (!x11)
        return;

    bm_x11_window_destroy(&x11->window);

    if (x11->display)
        XCloseDisplay(x11->display);

    free(x11);
    menu->renderer->internal = NULL;
}

static bool
constructor(struct bm_menu *menu)
{
    if (!getenv("BEMENU_BACKEND") && (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET")))
        return false;

    struct x11 *x11;
    if (!(menu->renderer->internal = x11 = calloc(1, sizeof(struct x11))))
        goto fail;

    if (!(x11->display = XOpenDisplay(NULL)))
        goto fail;

    if (!bm_x11_window_create(&x11->window, x11->display))
        goto fail;

    XSetClassHint(x11->window.display, x11->window.drawable, (XClassHint[]){{.res_name = (menu->title ? menu->title : "bemenu"), .res_class = "bemenu"}});

    x11->window.align = menu->align;
    bm_x11_window_set_monitor(&x11->window, menu->monitor);

    x11->window.notify.render = bm_cairo_paint;
    return true;

fail:
    destructor(menu);
    return false;
}

BM_PUBLIC extern const char*
register_renderer(struct render_api *api)
{
    api->constructor = constructor;
    api->destructor = destructor;
    api->get_displayed_count = get_displayed_count;
    api->poll_key = poll_key;
    api->render = render;
    api->set_align = set_align;
    api->set_y_offset = set_y_offset;
    api->set_width = set_width;
    api->set_monitor = set_monitor;
    api->grab_keyboard = grab_keyboard;
    api->priorty = BM_PRIO_GUI;
    api->version = BM_PLUGIN_VERSION;
    return "x11";
}

/* vim: set ts=8 sw=4 tw=0 :*/
