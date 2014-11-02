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

    uint32_t count;
    bm_menu_get_filtered_items(menu, &count);
    uint32_t lines = (count < menu->lines ? count : menu->lines) + 1;
    bm_wl_window_render(&wayland->window, menu, lines);

    if (wl_display_dispatch(wayland->display) < 0)
        wayland->input.sym = XKB_KEY_Escape;
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland && unicode);
    *unicode = 0;

    if (wayland->input.sym == XKB_KEY_NoSymbol)
        return BM_KEY_UNICODE;

    uint32_t mods = wayland->input.modifiers;
    *unicode = xkb_state_key_get_utf32(wayland->input.xkb.state, wayland->input.code);

    if (!*unicode && wayland->input.code == 23 && (mods & MOD_SHIFT))
        return BM_KEY_SHIFT_TAB;

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
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_UP : BM_KEY_PAGE_UP);

        case XKB_KEY_SunPageDown:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_PAGE_DOWN : BM_KEY_PAGE_DOWN);

        case XKB_KEY_BackSpace:
            return BM_KEY_BACKSPACE;

        case XKB_KEY_Delete:
            return (mods & MOD_SHIFT ? BM_KEY_LINE_DELETE_LEFT : BM_KEY_DELETE);

        case XKB_KEY_Tab:
            return (mods & MOD_SHIFT ? BM_KEY_SHIFT_TAB : BM_KEY_TAB);

        case XKB_KEY_Insert:
            return BM_KEY_SHIFT_RETURN;

        case XKB_KEY_Return:
            return (mods & MOD_CTRL ? BM_KEY_CONTROL_RETURN : (mods & MOD_SHIFT ? BM_KEY_SHIFT_RETURN : BM_KEY_RETURN));

        case XKB_KEY_Escape:
            return BM_KEY_ESCAPE;

        case XKB_KEY_p:
            return (mods & MOD_CTRL ? BM_KEY_UP : BM_KEY_UNICODE);

        case XKB_KEY_n:
            return (mods & MOD_CTRL ? BM_KEY_DOWN : BM_KEY_UNICODE);

        case XKB_KEY_l:
            return (mods & MOD_CTRL ? BM_KEY_LEFT : BM_KEY_UNICODE);

        case XKB_KEY_f:
            return (mods & MOD_CTRL ? BM_KEY_RIGHT : BM_KEY_UNICODE);

        case XKB_KEY_a:
            return (mods & MOD_CTRL ? BM_KEY_HOME : BM_KEY_UNICODE);

        case XKB_KEY_e:
            return (mods & MOD_CTRL ? BM_KEY_END : BM_KEY_UNICODE);

        case XKB_KEY_h:
            return (mods & MOD_CTRL ? BM_KEY_BACKSPACE : BM_KEY_UNICODE);

        case XKB_KEY_u:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_LEFT : BM_KEY_UNICODE);

        case XKB_KEY_k:
            return (mods & MOD_CTRL ? BM_KEY_LINE_DELETE_RIGHT : BM_KEY_UNICODE);

        case XKB_KEY_w:
            return (mods & MOD_CTRL ? BM_KEY_WORD_DELETE : BM_KEY_UNICODE);

        default: break;
    }

    return BM_KEY_UNICODE;
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;
    assert(wayland);
    return wayland->window.displayed;
}

static void
destructor(struct bm_menu *menu)
{
    struct wayland *wayland = menu->renderer->internal;

    if (!wayland)
        return;

    bm_wl_window_destroy(&wayland->window);
    bm_wl_registry_destroy(wayland);

    xkb_context_unref(wayland->input.xkb.context);

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
    api->version = BM_PLUGIN_VERSION;
    return "wayland";
}

/* vim: set ts=8 sw=4 tw=0 :*/
