#include "internal.h"
#include "version.h"
#include "wayland.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static void
render(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    bm_wl_window_render(&wayland->window, menu);
    wl_display_dispatch(wayland->display);
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland && unicode);
    *unicode = 0;

    if (wayland->input.sym == XKB_KEY_NoSymbol)
        return BM_KEY_UNICODE;

    *unicode = xkb_state_key_get_utf32(wayland->input.xkb.state, wayland->input.code);

    switch (wayland->input.sym) {
        case XKB_KEY_Up:
            return BM_KEY_UP;

        case XKB_KEY_Down:
            return BM_KEY_DOWN;

        case XKB_KEY_Left:
            return BM_KEY_LEFT;

        case XKB_KEY_Right:
            return BM_KEY_RIGHT;

        case XKB_KEY_Home:
            return BM_KEY_HOME;

        case XKB_KEY_End:
            return BM_KEY_END;

        case XKB_KEY_SunPageUp:
            return BM_KEY_PAGE_UP;

        case XKB_KEY_SunPageDown:
            return BM_KEY_PAGE_DOWN;

        case XKB_KEY_BackSpace:
            return BM_KEY_BACKSPACE;

        case XKB_KEY_Delete:
            return BM_KEY_DELETE;

        case XKB_KEY_Tab:
            return BM_KEY_TAB;

        case XKB_KEY_Insert:
            return BM_KEY_SHIFT_RETURN;

        case XKB_KEY_Return:
            return BM_KEY_RETURN;

        case XKB_KEY_Escape:
            return BM_KEY_ESCAPE;

        default: break;
    }

    return BM_KEY_UNICODE;
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    return wayland->window.height / 12;
}

static void
destructor(struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;

    if (!wayland)
        return;

    bm_wl_window_destroy(&wayland->window);
    bm_wl_registry_destroy(wayland);

    if (wayland->display) {
        wl_display_flush(wayland->display);
        wl_display_disconnect(wayland->display);
    }

    free(wayland);
    menu->renderer->internal = NULL;
}

static bool
constructor(struct bm_menu *menu)
{
    struct wayland *wayland;
    if (!(menu->renderer->internal = wayland = calloc(1, sizeof(struct wayland))))
        goto fail;

    if (!(wayland->display = wl_display_connect(NULL)))
        goto fail;

    if (!(wayland->input.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS)))
        goto fail;

    if (!bm_wl_registry_register(wayland))
        goto fail;

    struct wl_surface *surface;
    if (!(surface = wl_compositor_create_surface(wayland->compositor)))
        goto fail;

    if (!bm_wl_window_create(&wayland->window, wayland->shm, wayland->shell, wayland->xdg_shell, surface))
        goto fail;

    wayland->window.notify.render = bm_cairo_paint;
    return true;

fail:
    destructor(menu);
    return false;
}

extern const char*
register_renderer(struct render_api *api)
{
    api->constructor = constructor;
    api->destructor = destructor;
    api->get_displayed_count = get_displayed_count;
    api->poll_key = poll_key;
    api->render = render;
    api->prioritory = BM_PRIO_GUI;
    api->version = BM_VERSION;
    return "wayland";
}

/* vim: set ts=8 sw=4 tw=0 :*/
