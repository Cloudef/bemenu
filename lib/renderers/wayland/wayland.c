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
render_windows_if_pending(struct bm_menu *menu, struct wayland *wayland) {
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->render_pending)
            bm_wl_window_render(window, wayland->display, menu);
    }
    wl_display_flush(wayland->display);
}

static bool
wait_for_events(struct wayland *wayland) {
    wl_display_dispatch_pending(wayland->display);

    if (wl_display_flush(wayland->display) < 0 && errno != EAGAIN)
        return false;

    struct epoll_event ep[16];
    uint32_t num = epoll_wait(efd, ep, 16, -1);
    for (uint32_t i = 0; i < num; ++i) {
        if (ep[i].data.ptr == &wayland->fds.display) {
            if (ep[i].events & EPOLLERR || ep[i].events & EPOLLHUP ||
               ((ep[i].events & EPOLLIN) && wl_display_dispatch(wayland->display) < 0))
                return false;
        } else if (ep[i].data.ptr == &wayland->fds.repeat) {
            bm_wl_repeat(wayland);
        }
    }

    return true;
}

static void
schedule_windows_render_if_dirty(struct bm_menu *menu, struct wayland *wayland) {
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->render_pending) {
            // This does not happen during normal execution, but only when the windows need to
            // be(re)created. We need to do the render ASAP (not schedule it) because otherwise,
            // since we lack a window, we may not receive further events and will get deadlocked
            render_windows_if_pending(menu, wayland);
        }
        if (menu->dirty) {
            bm_wl_window_schedule_render(window);
        }
    }

    menu->dirty = false;
}

static bool
render(struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;

    schedule_windows_render_if_dirty(menu, wayland);
    if (!wait_for_events(wayland))
        return false;
    render_windows_if_pending(menu, wayland);

    return true;
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland && unicode);
    *unicode = 0;

    if (wayland->input.sym == XKB_KEY_NoSymbol)
        return BM_KEY_NONE;

    xkb_keysym_t sym = wayland->input.sym;
    uint32_t mods = wayland->input.modifiers;
    *unicode = xkb_state_key_get_utf32(wayland->input.xkb.state, wayland->input.code);

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

        case XKB_KEY_ISO_Left_Tab: //shift tab gives this sym
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

        case XKB_KEY_b:
            return (mods & MOD_CTRL ? BM_KEY_LEFT : BM_KEY_UNICODE);

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

struct bm_pointer
poll_pointer(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    struct input *input = &wayland->input;
    struct pointer_event *event = &input->pointer_event;
    assert(wayland && event);

    struct bm_pointer bm_pointer = {0};

    bm_pointer.event_mask = event->event_mask;
    bm_pointer.pos_x = wl_fixed_to_int(event->surface_x);
    bm_pointer.pos_y = wl_fixed_to_int(event->surface_y);
    bm_pointer.time = event->time;
    bm_pointer.axes[0].valid = event->axes[0].valid;
    bm_pointer.axes[0].value = event->axes[0].value;
    bm_pointer.axes[0].discrete = event->axes[0].discrete;
    bm_pointer.axes[1].valid = event->axes[1].valid;
    bm_pointer.axes[1].value = event->axes[1].value;
    bm_pointer.axes[1].discrete = event->axes[1].discrete;
    bm_pointer.axis_source = event->axis_source;

    bm_pointer.button = BM_POINTER_KEY_NONE;
    switch (event->button) {
        case BTN_LEFT:
            bm_pointer.button = BM_POINTER_KEY_PRIMARY;
            break;
    }

    if (event->state & WL_POINTER_BUTTON_STATE_PRESSED) {
        bm_pointer.state = POINTER_STATE_PRESSED;
    }
    if (event->state & WL_POINTER_BUTTON_STATE_RELEASED) {
        bm_pointer.state = POINTER_STATE_RELEASED;
    }

    memset(event, 0, sizeof(*event));
    return bm_pointer;
}

struct bm_touch
poll_touch(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    struct input *input = &wayland->input;
    struct touch_event *event = &input->touch_event;
    assert(wayland && event);

    struct bm_touch bm_touch;

    for (size_t i = 0; i < 2; ++i) {
        struct touch_point *point = &event->points[i];

        if (!point->valid) {
            bm_touch.points[i].event_mask = 0;
            continue;
        }

        bm_touch.points[i].event_mask = point->event_mask;
        bm_touch.points[i].pos_x = wl_fixed_to_int(point->surface_x);
        bm_touch.points[i].pos_y = wl_fixed_to_int(point->surface_y);
        bm_touch.points[i].start_x = wl_fixed_to_int(point->surface_start_x);
        bm_touch.points[i].start_y = wl_fixed_to_int(point->surface_start_y);
        bm_touch.points[i].major = point->major;
        bm_touch.points[i].minor = point->minor;
        bm_touch.points[i].orientation = point->orientation;

        if (point->event_mask & TOUCH_EVENT_UP) {
            point->valid = false;
            point->event_mask = 0;
        }
    }

    return bm_touch;
}

void
release_touch(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    struct input *input = &wayland->input;
    struct touch_event *event = &input->touch_event;
    assert(wayland && event);

    for (size_t i = 0; i < 2; ++i) {
        struct touch_point *point = &event->points[i];
        point->valid = false;
    }
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

static uint32_t
get_height(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    uint32_t max = 0;
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->displayed > max)
            max = window->height;
    }
    return max;
}

static uint32_t
get_width(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    uint32_t max = 0;
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        if (window->displayed > max)
            max = window->width;
    }
    return max;
}

static void
set_width(const struct bm_menu *menu, uint32_t margin, float factor)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_set_width(window, wayland->display, margin, factor);
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
set_y_offset(const struct bm_menu *menu, int32_t y_offset)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);

    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_set_y_offset(window, wayland->display, y_offset);
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

void
wl_surface_enter(void *data, struct wl_surface *wl_surface,
                 struct wl_output *wl_output)
{
    (void)wl_surface;
    struct window *window = data;
    struct wayland *wayland = window->wayland;

    struct output *output;
    wl_list_for_each(output, &wayland->outputs, link) {
        if (output->output == wl_output) {
            struct surf_output *surf_output = calloc(1, sizeof(struct surf_output));
            surf_output->output = output;
            wl_list_insert(&window->surf_outputs, &surf_output->link);
            break;
        }
    }

    window_update_output(window);
}

void
wl_surface_leave(void *data, struct wl_surface *wl_surface,
                 struct wl_output *wl_output)
{
    (void)wl_surface;
    struct window *window = data;

    struct surf_output *surf_output;
    wl_list_for_each(surf_output, &window->surf_outputs, link) {
        if (surf_output->output->output == wl_output) {
            wl_list_remove(&surf_output->link);
            break;
        }
    }

    window_update_output(window);
}

static const struct wl_surface_listener surface_listener = {
    .enter = wl_surface_enter,
    .leave = wl_surface_leave,
};

static void
destroy_windows(struct wayland *wayland)
{
    struct window *window;
    wl_list_for_each(window, &wayland->windows, link) {
        bm_wl_window_destroy(window);
    }
    wl_list_init(&wayland->windows);
}

void
window_update_output(struct window *window)
{
    int32_t max_scale = 1;
    uint32_t min_max_height = 0;

    struct surf_output *surf_output;
    wl_list_for_each(surf_output, &window->surf_outputs, link) {
        if (surf_output->output->scale > max_scale) {
            max_scale = surf_output->output->scale;
        }
        if (min_max_height == 0 || surf_output->output->height < min_max_height) {
            min_max_height = surf_output->output->height;
        };
    }

    if (min_max_height != window->max_height) {
        window->max_height = min_max_height;
    }

    if (max_scale != window->scale) {
        window->scale = max_scale;
    }
}

void
recreate_windows(const struct bm_menu *menu, struct wayland *wayland)
{
    destroy_windows(wayland);

    struct window *window = calloc(1, sizeof(struct window));
    wl_list_init(&window->surf_outputs);
    window->wayland = wayland;
    window->align = menu->align;
    window->hmargin_size = menu->hmargin_size;
    window->width_factor = menu->width_factor;

    // TODO: this should not be necessary, but Sway 1.8.1 does not trigger event
    // surface.enter before we actually need to render the first frame.
    window->scale = 1;
    window->max_height = 640;

    struct wl_surface *surface = NULL;
    if (!(surface = wl_compositor_create_surface(wayland->compositor)))
        goto fail;

    wl_surface_add_listener(surface, &surface_listener, window);

    struct output *output = NULL;
    if (wayland->selected_output) {
        fprintf(stderr, "selected output\n");
        output = wayland->selected_output;
    };

    struct wl_output *wl_output = NULL;
    if (output)
        wl_output = output->output;

    if (!bm_wl_window_create(window, wayland->display, wayland->shm,
                             wl_output, wayland->layer_shell, surface)) {
        free(window);
        goto fail;
    }

    window->notify.render = bm_cairo_paint;
    window->render_pending = true;
    wl_list_insert(&wayland->windows, &window->link);

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

    if (!monitor_name) {
        return;
    }

    struct output *output;
    wl_list_for_each(output, &wayland->outputs, link) {
        if (0 == strcmp(monitor_name, output->name)) {
            wayland->selected_output = output;
            recreate_windows(menu, wayland);
            return;
        }
    }
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
    api->get_height = get_height;
    api->get_width = get_width;
    api->poll_key = poll_key;
    api->poll_pointer = poll_pointer;
    api->poll_touch = poll_touch;
    api->release_touch = release_touch;
    api->render = render;
    api->set_align = set_align;
    api->set_y_offset = set_y_offset;
    api->set_width = set_width;
    api->grab_keyboard = grab_keyboard;
    api->set_overlap = set_overlap;
    api->set_monitor = set_monitor;
    api->set_monitor_name = set_monitor_name;
    api->priorty = BM_PRIO_GUI;
    api->version = BM_PLUGIN_VERSION;
    return "wayland";
}

/* vim: set ts=8 sw=4 tw=0 :*/
