#include "internal.h"
#include "wayland.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>

static int efd;

static void
render(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    wl_display_dispatch_pending(wayland->display);

    if (wl_display_flush(wayland->display) < 0 && errno != EAGAIN) {
        wayland->input.sym = XKB_KEY_Escape;
        return;
    }

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->render_pending)
            bm_wl_window_render(window, wayland->display, menu);
    }
    wl_display_flush(wayland->display);

    struct epoll_event ep[16];
    uint32_t num = epoll_wait(efd, ep, 16, -1);
    for (uint32_t i = 0; i < num; ++i) {
        if (ep[i].data.ptr == &wayland->fds.display) {
            if (ep[i].events & EPOLLERR || ep[i].events & EPOLLHUP ||
               ((ep[i].events & EPOLLIN) && wl_display_dispatch(wayland->display) < 0))
                wayland->input.sym = XKB_KEY_Escape;
        } else if (ep[i].data.ptr == &wayland->fds.repeat) {
            bm_wl_repeat(wayland);
        }
    }

    if (wayland->input.code != wayland->input.last_code) {
        wl_list_for_each(window, &wayland->windows, link) {
            bm_wl_window_schedule_render(window);
        }

        wayland->input.last_code = wayland->input.code;
    }
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland && unicode);
    *unicode = 0;

    if (wayland->input.sym == XKB_KEY_NoSymbol)
        return BM_KEY_UNICODE;

    xkb_keysym_t sym = wayland->input.sym;
    uint32_t mods = wayland->input.modifiers;
    *unicode = xkb_state_key_get_utf32(wayland->input.xkb.state, wayland->input.code);

    if (!*unicode && wayland->input.code == 23 && (mods & MOD_SHIFT))
        return BM_KEY_SHIFT_TAB;

    wayland->input.sym = XKB_KEY_NoSymbol;
    wayland->input.code = 0;

    if (!wayland->input.key_pending)
	    return BM_KEY_UNICODE;
    wayland->input.key_pending = false;

    switch (sym) {
        case XKB_KEY_Up:
            return BM_KEY_UP;

        case XKB_KEY_Down:
            return BM_KEY_DOWN;

        case XKB_KEY_Left:
            return (mods & MOD_SHIFT ? BM_KEY_UP : BM_KEY_LEFT);

        case XKB_KEY_Right:
            return (mods & MOD_SHIFT ? BM_KEY_DOWN : BM_KEY_RIGHT);

        case XKB_KEY_Home:
            return BM_KEY_HOME;

        case XKB_KEY_End:
            return BM_KEY_END;

        case XKB_KEY_SunPageUp:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_UP : BM_KEY_PAGE_UP);

        case XKB_KEY_less:
            return (mods & MOD_ALT ? BM_KEY_SHIFT_PAGE_UP : BM_KEY_UNICODE);

        case XKB_KEY_SunPageDown:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_DOWN : BM_KEY_PAGE_DOWN);

        case XKB_KEY_greater:
            return (mods & MOD_ALT ? BM_KEY_SHIFT_PAGE_DOWN : BM_KEY_UNICODE);

        case XKB_KEY_v:
            return (mods & MOD_CTRL ? BM_KEY_PAGE_DOWN : (mods & MOD_ALT ? BM_KEY_PAGE_UP : BM_KEY_UNICODE));

        case XKB_KEY_BackSpace:
            return BM_KEY_BACKSPACE;

        case XKB_KEY_Delete:
            return (mods & MOD_SHIFT ? BM_KEY_LINE_DELETE_LEFT : BM_KEY_DELETE);

        case XKB_KEY_Tab:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_TAB : BM_KEY_TAB);

        case XKB_KEY_Insert:
            return BM_KEY_SHIFT_RETURN;

        case XKB_KEY_KP_Enter:
        case XKB_KEY_Return:
            return (mods & MOD_CTRL ? BM_KEY_CONTROL_RETURN : (mods & MOD_SHIFT ? BM_KEY_SHIFT_RETURN : BM_KEY_RETURN));

        case XKB_KEY_g:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XKB_KEY_c:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XKB_KEY_bracketleft:
            if (!(mods & MOD_CTRL)) return BM_KEY_UNICODE;
            // fall through
        case XKB_KEY_Escape:
            return BM_KEY_ESCAPE;

        case XKB_KEY_p:
            return (mods & MOD_CTRL ? BM_KEY_UP : BM_KEY_UNICODE);

        case XKB_KEY_n:
            return (mods & MOD_CTRL ? BM_KEY_DOWN : BM_KEY_UNICODE);

        case XKB_KEY_l:
            return (mods & MOD_CTRL ? BM_KEY_LEFT : (mods & MOD_ALT ? BM_KEY_DOWN : BM_KEY_UNICODE));

        case XKB_KEY_f:
            return (mods & MOD_CTRL ? BM_KEY_RIGHT : BM_KEY_UNICODE);

        case XKB_KEY_a:
            return (mods & MOD_CTRL ? BM_KEY_HOME : BM_KEY_UNICODE);

        case XKB_KEY_e:
            return (mods & MOD_CTRL ? BM_KEY_END : BM_KEY_UNICODE);

        case XKB_KEY_h:
            return (mods & MOD_CTRL ? BM_KEY_BACKSPACE : (mods & MOD_ALT ? BM_KEY_UP : BM_KEY_UNICODE));

        case XKB_KEY_u:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_LEFT : (mods & MOD_ALT ? BM_KEY_PAGE_UP : BM_KEY_UNICODE));

        case XKB_KEY_k:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_RIGHT : (mods & MOD_ALT ? BM_KEY_UP : BM_KEY_UNICODE));

        case XKB_KEY_w:
            return (mods & MOD_CTRL ? BM_KEY_WORD_DELETE : BM_KEY_UNICODE);

        case XKB_KEY_j:
            return (mods & MOD_ALT ? BM_KEY_DOWN : BM_KEY_UNICODE);

        case XKB_KEY_d:
            return (mods & MOD_ALT ? BM_KEY_PAGE_DOWN : BM_KEY_UNICODE);

        case XKB_KEY_m:
            return (mods & MOD_CTRL ? BM_KEY_RETURN : BM_KEY_UNICODE);

        case XKB_KEY_y:
            return (mods & MOD_CTRL ? BM_KEY_PASTE : BM_KEY_UNICODE);

        case XKB_KEY_1:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_1;
            break;
        case XKB_KEY_2:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_2;
            break;
        case XKB_KEY_3:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_3;
            break;
        case XKB_KEY_4:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_4;
            break;
        case XKB_KEY_5:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_5;
            break;
        case XKB_KEY_6:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_6;
            break;
        case XKB_KEY_7:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_7;
            break;
        case XKB_KEY_8:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_8;
            break;
        case XKB_KEY_9:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_9;
            break;
        case XKB_KEY_0:
            if ((mods & MOD_ALT)) return BM_KEY_CUSTOM_10;
            break;

        default: break;
    }

    return BM_KEY_UNICODE;
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    uint32_t max = 0;
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->displayed > max)
            max = window->displayed;
    }
    return max;
}

static void
set_hmargin_size(const struct bm_menu *menu, uint32_t margin)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_set_hmargin_size(window, wayland->display, margin);
    }
}

static void
set_align(const struct bm_menu *menu, enum bm_align align)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_set_align(window, wayland->display, align);
    }
}

static void
grab_keyboard(const struct bm_menu *menu, bool grab)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_grab_keyboard(window, wayland->display, grab);
    }
}

static void
set_overlap(const struct bm_menu *menu, bool overlap)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_set_overlap(window, wayland->display, overlap);
    }
}

static void
destroy_windows(struct wayland *wayland)
{
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_destroy(window);
    }
    wl_list_init(&wayland->windows);
}

static void
recreate_windows(const struct bm_menu *menu, struct wayland *wayland)
{
    destroy_windows(wayland);

    int32_t monitors = 0;
    struct output *output;
    wl_list_for_each(output, &wayland->outputs, link)
        monitors++;

    int32_t monitor = 0;
    wl_list_for_each(output, &wayland->outputs, link) {

        if (!menu->monitor_name) {
            if (menu->monitor > -1) {
                 if (menu->monitor < monitors && monitor != menu->monitor) {
                    ++monitor;
                    continue;
                }
            }
        } else if (strcmp(menu->monitor_name, output->name)) {
            continue;
        }

        struct wl_surface *surface;
        if (!(surface = wl_compositor_create_surface(wayland->compositor)))
            goto fail;

        wl_surface_set_buffer_scale(surface, output->scale);

        struct window *window = calloc(1, sizeof(struct window));
        window->align = menu->align;
        window->hmargin_size = menu->hmargin_size;

        const char *scale = getenv("BEMENU_SCALE");
        if (scale) {
            window->scale = fmax(strtof(scale, NULL), 1.0f);
        } else {
            window->scale = output->scale;
        }

        if (!bm_wl_window_create(window, wayland->display, wayland->shm, (menu->monitor == -1) ? NULL : output->output, wayland->layer_shell, surface))
            free(window);

        window->notify.render = bm_cairo_paint;
        window->max_height = output->height;
        window->render_pending = true;
        wl_list_insert(&wayland->windows, &window->link);
        if (menu->monitor != -2) break;
    }

    set_overlap(menu, menu->overlap);
    grab_keyboard(menu, menu->grabbed);
    return;

fail:
    // This is non handlable if fails on recreation
    fprintf(stderr, "wayland window creation failed :/\n");
    abort();
}

static void
set_monitor(const struct bm_menu *menu, int32_t monitor)
{
    (void)monitor;
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    recreate_windows(menu, wayland);
}

static void
set_monitor_name(const struct bm_menu *menu, char *monitor_name)
{
    (void)monitor_name;
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    recreate_windows(menu, wayland);
}

static void
destructor(struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;

    if (!wayland)
        return;

    destroy_windows(wayland);
    bm_wl_registry_destroy(wayland);

    xkb_context_unref(wayland->input.xkb.context);

    if (wayland->display) {
        epoll_ctl(efd, EPOLL_CTL_DEL, wayland->fds.repeat, NULL);
        epoll_ctl(efd, EPOLL_CTL_DEL, wayland->fds.display, NULL);
        close(wayland->fds.repeat);
        wl_display_flush(wayland->display);
        wl_display_disconnect(wayland->display);
    }

    free(wayland);
    menu->renderer->internal = NULL;
}

static bool
constructor(struct bm_menu *menu)
{
    if (!getenv("WAYLAND_DISPLAY") && !getenv("WAYLAND_SOCKET"))
        return false;

    struct wayland *wayland;
    if (!(menu->renderer->internal = wayland = calloc(1, sizeof(struct wayland))))
        goto fail;

    wl_list_init(&wayland->windows);
    wl_list_init(&wayland->outputs);

    if (!(wayland->display = wl_display_connect(NULL)))
        goto fail;

    if (!(wayland->input.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS)))
        goto fail;

    if (!bm_wl_registry_register(wayland))
        goto fail;

    wayland->fds.display = wl_display_get_fd(wayland->display);
    wayland->fds.repeat = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    wayland->input.repeat_fd = &wayland->fds.repeat;
    wayland->input.key_pending = false;
    recreate_windows(menu, wayland);

    if (!efd && (efd = epoll_create1(EPOLL_CLOEXEC)) < 0)
        goto fail;

    struct epoll_event ep;
    ep.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ep.data.ptr = &wayland->fds.display;
    epoll_ctl(efd, EPOLL_CTL_ADD, wayland->fds.display, &ep);

    struct epoll_event ep2;
    ep2.events = EPOLLIN;
    ep2.data.ptr = &wayland->fds.repeat;
    epoll_ctl(efd, EPOLL_CTL_ADD, wayland->fds.repeat, &ep2);
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
    api->set_hmargin_size = set_hmargin_size;
    api->grab_keyboard = grab_keyboard;
    api->set_overlap = set_overlap;
    api->set_monitor = set_monitor;
    api->set_monitor_name = set_monitor_name;
    api->priorty = BM_PRIO_GUI;
    api->version = BM_PLUGIN_VERSION;
    return "wayland";
}

/* vim: set ts=8 sw=4 tw=0 :*/
