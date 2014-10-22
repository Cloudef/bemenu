#define _DEFAULT_SOURCE
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <cairo/cairo.h>

#define MOD_SHIFT_MASK      0x01
#define MOD_ALT_MASK        0x02
#define MOD_CONTROL_MASK    0x04

struct xkb {
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    xkb_mod_mask_t control_mask;
    xkb_mod_mask_t alt_mask;
    xkb_mod_mask_t shift_mask;
    xkb_keysym_t sym;
    uint32_t key;
    uint32_t modifiers;
};

struct cairo {
    cairo_t *cr;
    cairo_surface_t *surface;
};

struct buffer {
    struct cairo cairo;
    struct wl_buffer *buffer;
    int busy;
};

static struct wayland {
    struct bm_menu *menu;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
#if 0
    struct xdg_shell *shell;
#else
    struct wl_shell *shell;
#endif
    struct xkb xkb;
    struct buffer buffers[2];
    uint32_t width, height;
    uint32_t formats;
} wayland;

// XXX: move to utils.c if reused
static char*
csprintf(const char *fmt, ...)
{
   assert(fmt);

   va_list args;
   va_start(args, fmt);
   size_t len = vsnprintf(NULL, 0, fmt, args) + 1;
   va_end(args);

   char *buffer;
   if (!(buffer = calloc(1, len)))
      return NULL;

   va_start(args, fmt);
   vsnprintf(buffer, len, fmt, args);
   va_end(args);
   return buffer;
}

static int
set_cloexec_or_close(int fd)
{
   if (fd == -1)
      return -1;

   long flags = fcntl(fd, F_GETFD);
   if (flags == -1)
      goto err;

   if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
      goto err;

   return fd;

err:
   close(fd);
   return -1;
}

static int
create_tmpfile_cloexec(char *tmpname)
{
   int fd;

#ifdef HAVE_MKOSTEMP
   if ((fd = mkostemp(tmpname, O_CLOEXEC)) >= 0)
      unlink(tmpname);
#else
   if ((fd = mkstemp(tmpname)) >= 0) {
      fd = set_cloexec_or_close(fd);
      unlink(tmpname);
   }
#endif

   return fd;
}

static int
os_create_anonymous_file(off_t size)
{
   static const char template[] = "/bemenu-shared-XXXXXX";
   int fd;
   int ret;

   const char *path;
   if (!(path = getenv("XDG_RUNTIME_DIR")) || strlen(path) <= 0) {
      errno = ENOENT;
      return -1;
   }

   char *name;
   int ts = (path[strlen(path) - 1] == '/');
   if (!(name = csprintf("%s%s%s", path, (ts ? "" : "/"), template)))
      return -1;

   fd = create_tmpfile_cloexec(name);
   free(name);

   if (fd < 0)
      return -1;

#ifdef HAVE_POSIX_FALLOCATE
   if ((ret = posix_fallocate(fd, 0, size)) != 0) {
      close(fd);
      errno = ret;
      return -1;
   }
#else
   if ((ret = ftruncate(fd, size)) < 0) {
      close(fd);
      return -1;
   }
#endif

   return fd;
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
    struct buffer *mybuf = data;
    mybuf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_release
};

static bool
create_shm_buffer(struct buffer *buffer, int32_t width, int32_t height, uint32_t format)
{
    uint32_t stride = width * 4;
    uint32_t size = stride * height;

    int fd;
    if ((fd = os_create_anonymous_file(size)) < 0) {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
        return false;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        return false;
    }

    // FIXME: error handling
    struct wl_shm_pool *pool = wl_shm_create_pool(wayland.shm, fd, size);
    buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);
    wl_shm_pool_destroy(pool);
    close(fd);

    buffer->cairo.surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride);
    buffer->cairo.cr = cairo_create(buffer->cairo.surface);
    return true;
}

static struct buffer*
next_buffer(void)
{
    struct buffer *buffer;

    if (!wayland.buffers[0].busy)
        buffer = &wayland.buffers[0];
    else if (!wayland.buffers[1].busy)
        buffer = &wayland.buffers[1];
    else
        return NULL;

    if (!buffer->buffer) {
        if (!create_shm_buffer(buffer, wayland.width, wayland.height, WL_SHM_FORMAT_ARGB8888))
            return NULL;
    }

    return buffer;
}

static bool
resize_buffer(char **buffer, size_t *osize, size_t nsize)
{
    assert(buffer);
    assert(osize);

    if (nsize == 0 || nsize <= *osize)
        return false;

    void *tmp;
    if (!*buffer || !(tmp = realloc(*buffer, nsize))) {
        if (!(tmp = malloc(nsize)))
            return 0;

        if (*buffer) {
            memcpy(tmp, *buffer, *osize);
            free(*buffer);
        }
    }

    *buffer = tmp;
    *osize = nsize;
    return true;
}

#if __GNUC__
__attribute__((format(printf, 5, 6)))
#endif
static int32_t
draw_line(struct cairo *c, int32_t pair, int32_t x, int32_t y, const char *format, ...)
{
    static size_t blen = 0;
    static char *buffer = NULL;

    va_list args;
    va_start(args, format);
    size_t nlen = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if ((!buffer || nlen > blen) && !resize_buffer(&buffer, &blen, nlen + 1))
        return;

    va_start(args, format);
    vsnprintf(buffer, blen, format, args);
    va_end(args);

    cairo_text_extents_t te;
    switch (pair) {
        case 2:
            cairo_set_source_rgb(c->cr, 1.0, 0.0, 0.0);
            break;
        case 1:
            cairo_set_source_rgb(c->cr, 1.0, 0.0, 0.0);
            break;
        case 0:
            cairo_set_source_rgb(c->cr, 1.0, 1.0, 1.0);
            break;
    };

    cairo_select_font_face(c->cr, "Terminus", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(c->cr, 12);
    cairo_text_extents(c->cr, buffer, &te);
    cairo_move_to(c->cr, x, y * te.height + te.height);
    cairo_show_text(c->cr, buffer);
    return x + te.x_advance;
}

static void
paint(struct cairo *c, int width, int height, uint32_t time)
{
    cairo_set_source_rgb(c->cr, 18.0f / 255.0f, 18 / 255.0f, 18.0f / 255.0f);
    cairo_rectangle(c->cr, 0, 0, width, height);
    cairo_fill(c->cr);

    struct bm_menu *menu = wayland.menu;

    int32_t x = 0;
    if (menu->title)
        x = draw_line(c, 1, x, 0, "%s ", menu->title);
    if (menu->filter)
        x = draw_line(c, 1, x, 0, "%s", menu->filter);

    uint32_t lines = height / 8;
    uint32_t count, cl = 1;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    for (uint32_t i = (menu->index / (lines - 1)) * (lines - 1); i < count && cl < lines; ++i) {
        bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));
        int32_t color = (highlighted ? 2 : (bm_menu_item_is_selected(menu, items[i]) ? 1 : 0));
        draw_line(c, color, 0, cl++, "%s%s", (highlighted ? ">> " : "   "), (items[i]->text ? items[i]->text : ""));
    }
}

static void
redraw(void *data, uint32_t time)
{
    struct wayland *d = data;
    struct buffer *buffer;

    if (!(buffer = next_buffer()))
        return;

    paint(&buffer->cairo, d->width, d->height, time);
    wl_surface_attach(d->surface, buffer->buffer, 0, 0);
    wl_surface_commit(d->surface);
    buffer->busy = 1;
}

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    (void)wl_shm;
    struct wayland *d = data;
    d->formats |= (1 << format);
}

struct wl_shm_listener shm_listener = {
    .format = shm_format
};

#if 0
static void
xdg_shell_ping(void *data, struct xdg_shell *shell, uint32_t serial)
{
    (void)data;
    xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
    xdg_shell_ping,
};
#endif

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    struct wayland *d = data;

    if (!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(d->xkb.context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    munmap(map_str, size);
    close(fd);

    if (!keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return;
    }

    struct xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        fprintf(stderr, "failed to create XKB state\n");
        xkb_keymap_unref(keymap);
        return;
    }

    xkb_keymap_unref(d->xkb.keymap);
    xkb_state_unref(d->xkb.state);
    d->xkb.keymap = keymap;
    d->xkb.state = state;
    d->xkb.control_mask = 1 << xkb_keymap_mod_get_index(d->xkb.keymap, "Control");
    d->xkb.alt_mask = 1 << xkb_keymap_mod_get_index(d->xkb.keymap, "Mod1");
    d->xkb.shift_mask = 1 << xkb_keymap_mod_get_index(d->xkb.keymap, "Shift");
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
    (void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    (void)data, (void)keyboard, (void)serial, (void)surface;
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state_w)
{
    struct wayland *d = data;
    enum wl_keyboard_key_state state = state_w;

    if (!d->xkb.state)
        return;

    uint32_t code = key + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(d->xkb.state, code);

    d->xkb.key = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? code : 0);
    d->xkb.sym = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? sym : XKB_KEY_NoSymbol);

#if 0
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED &&
            key == input->repeat_key) {
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
        timerfd_settime(input->repeat_timer_fd, 0, &its, NULL);
    } else if (state == WL_KEYBOARD_KEY_STATE_PRESSED &&
            xkb_keymap_key_repeats(input->xkb.keymap, code)) {
        input->repeat_sym = sym;
        input->repeat_key = key;
        input->repeat_time = time;
        its.it_interval.tv_sec = input->repeat_rate_sec;
        its.it_interval.tv_nsec = input->repeat_rate_nsec;
        its.it_value.tv_sec = input->repeat_delay_sec;
        its.it_value.tv_nsec = input->repeat_delay_nsec;
        timerfd_settime(input->repeat_timer_fd, 0, &its, NULL);
    }
#endif
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    (void)keyboard, (void)serial;
    struct wayland *d = data;

    if (!d->xkb.keymap)
        return;

    xkb_state_update_mask(d->xkb.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    xkb_mod_mask_t mask = xkb_state_serialize_mods(d->xkb.state, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);

    d->xkb.modifiers = 0;
    if (mask & d->xkb.control_mask)
        d->xkb.modifiers |= MOD_CONTROL_MASK;
    if (mask & d->xkb.alt_mask)
        d->xkb.modifiers |= MOD_ALT_MASK;
    if (mask & d->xkb.shift_mask)
        d->xkb.modifiers |= MOD_SHIFT_MASK;
}

static void
keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
    (void)data, (void)keyboard, (void)rate, (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    struct wayland *d = data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
        d->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(d->keyboard, &keyboard_listener, data);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
        wl_keyboard_destroy(d->keyboard);
        d->keyboard = NULL;
    }
}

static void
seat_handle_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name
};

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    (void)version;
    struct wayland *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
#if 0
    } else if (strcmp(interface, "xdg_shell") == 0) {
        d->shell = wl_registry_bind(registry, id, &xdg_shell_interface, 1);
        xdg_shell_use_unstable_version(d->shell, XDG_VERSION);
        xdg_shell_add_listener(d->shell, &xdg_shell_listener, d);
#else
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
#endif
    } else if (strcmp(interface, "wl_seat") == 0) {
        d->seat = wl_registry_bind(registry, id, &wl_seat_interface, 4);
        wl_seat_add_listener(d->seat, &seat_listener, d);
    } else if (strcmp(interface, "wl_shm") == 0) {
        d->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        wl_shm_add_listener(d->shm, &shm_listener, d);
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void
render(const struct bm_menu *menu)
{
    (void)menu;
    redraw(&wayland, 0);
    wl_display_dispatch(wayland.display);
}

static enum bm_key
poll_key(const struct bm_menu *menu, unsigned int *unicode)
{
    (void)menu;
    assert(unicode);
    *unicode = 0;

    if (wayland.xkb.sym == XKB_KEY_NoSymbol)
        return BM_KEY_UNICODE;

    *unicode = xkb_state_key_get_utf32(wayland.xkb.state, wayland.xkb.key);

    switch (wayland.xkb.sym) {
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
    (void)menu;
    return wayland.height / 8;
}

static void
destructor(struct bm_menu *menu)
{
    (void)menu;

    if (wayland.shm)
        wl_shm_destroy(wayland.shm);

#if 0
    if (wayland.shell)
        xdg_shell_destroy(wayland.shell);
#endif

    if (wayland.compositor)
        wl_compositor_destroy(wayland.compositor);

    if (wayland.registry)
        wl_registry_destroy(wayland.registry);

    if (wayland.display) {
        wl_display_flush(wayland.display);
        wl_display_disconnect(wayland.display);
    }
}

static bool
constructor(struct bm_menu *menu)
{
    (void)menu;
    memset(&wayland, 0, sizeof(wayland));

    if (!(wayland.display = wl_display_connect(NULL)))
        goto fail;

    if (!(wayland.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS)))
        goto fail;

    wayland.width = 800;
    wayland.height = 480;

    wayland.registry = wl_display_get_registry(wayland.display);
    wl_registry_add_listener(wayland.registry, &registry_listener, &wayland);
    wl_display_roundtrip(wayland.display); // shm bind
    if (!wayland.shm)
        goto fail;

    wl_display_roundtrip(wayland.display); // shm formats
    if (!(wayland.formats & (1 << WL_SHM_FORMAT_ARGB8888)))
        goto fail;

    if (!(wayland.surface = wl_compositor_create_surface(wayland.compositor)))
        goto fail;

    if (!(wayland.shell_surface = wl_shell_get_shell_surface(wayland.shell, wayland.surface)))
        goto fail;

    wayland.menu = menu;
    wl_surface_damage(wayland.surface, 0, 0, wayland.width, wayland.height);
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
    return "wayland";
}

/* vim: set ts=8 sw=4 tw=0 :*/
