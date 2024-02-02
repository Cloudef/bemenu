#include "internal.h"
#include "wayland.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <math.h>

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
    static const char template[] = "bemenu-shared-XXXXXX";
    int fd;
    int ret;

    const char *path;
    if (!(path = getenv("XDG_RUNTIME_DIR")) || strlen(path) <= 0) {
        errno = ENOENT;
        return -1;
    }

    char *name;
    int ts = (path[strlen(path) - 1] == '/');
    if (!(name = bm_dprintf("%s%s%s", path, (ts ? "" : "/"), template)))
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
buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    (void)wl_buffer;
    struct buffer *buffer = data;
    buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

static void
destroy_buffer(struct buffer *buffer)
{
    if (buffer->buffer)
        wl_buffer_destroy(buffer->buffer);
    bm_cairo_destroy(&buffer->cairo);
    memset(buffer, 0, sizeof(struct buffer));
}

static bool
create_buffer(struct wl_shm *shm, struct buffer *buffer, int32_t width, int32_t height, uint32_t format, double scale)
{
    int fd = -1;
    struct wl_shm_pool *pool = NULL;
    uint32_t stride = width * 4;
    uint32_t size = stride * height;

    if ((fd = os_create_anonymous_file(size)) < 0) {
        fprintf(stderr, "wayland: creating a buffer file for %d B failed\n", size);
        return false;
    }

    void *data;
    if ((data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        fprintf(stderr, "wayland: mmap failed\n");
        close(fd);
        return false;
    }

    if (!(pool = wl_shm_create_pool(shm, fd, size))) {
        close(fd);
        return false;
    }

    if (!(buffer->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format)))
        goto fail;

    wl_shm_pool_destroy(pool);
    pool = NULL;

    close(fd);
    fd = -1;

    wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer);

    cairo_surface_t *surf;
    if (!(surf = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride)))
        goto fail;

    const char *env_scale = getenv("BEMENU_SCALE");
    if (env_scale) {
        buffer->cairo.scale = fmax(strtof(env_scale, NULL), 1.0f);
    } else {
        buffer->cairo.scale = scale;
    }

    if (!bm_cairo_create_for_surface(&buffer->cairo, surf)) {
        cairo_surface_destroy(surf);
        goto fail;
    }

    buffer->width = width;
    buffer->height = height;
    return true;

fail:
    if (fd > -1)
        close(fd);
    if (pool)
        wl_shm_pool_destroy(pool);
    destroy_buffer(buffer);
    return false;
}

static struct buffer*
next_buffer(struct window *window)
{
    assert(window);

    struct buffer *buffer = NULL;
    for (int32_t i = 0; i < 2; ++i) {
        if (window->buffers[i].busy)
            continue;

        buffer = &window->buffers[i];
        break;
    }

    if (!buffer)
        return NULL;

    if ((uint32_t) ceil(window->width * window->scale) != buffer->width || (uint32_t) ceil(window->height * window->scale) != buffer->height)
        destroy_buffer(buffer);

    if (!buffer->buffer && !create_buffer(window->shm, buffer, ceil(window->width * window->scale), ceil(window->height * window->scale), WL_SHM_FORMAT_ARGB8888, window->scale))
        return NULL;

    return buffer;
}

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    (void)time;
    struct window *window = data;
    wl_callback_destroy(callback);
    window->frame_cb = NULL;
    window->render_pending = true;
}

static const struct wl_callback_listener listener = {
    frame_callback
};

static uint32_t
get_align_anchor(enum bm_align align)
{
    uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;

    if(align == BM_ALIGN_TOP) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
    } else if(align == BM_ALIGN_CENTER) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    } else {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    }

    return anchor;
}

void
bm_wl_window_schedule_render(struct window *window)
{
    assert(window);
    if (window->frame_cb)
        return;

    window->frame_cb = wl_surface_frame(window->surface);
    wl_callback_add_listener(window->frame_cb, &listener, window);
    wl_surface_commit(window->surface);
}

void
bm_wl_window_render(struct window *window, struct wl_display *display, struct bm_menu *menu)
{
    (void)display;
    assert(window && menu);
    struct buffer *buffer;
    for (int tries = 0; tries < 2; ++tries) {
        if (!(buffer = next_buffer(window))) {
            fprintf(stderr, "could not get next buffer");
            exit(EXIT_FAILURE);
        }

        if (!window->notify.render)
            break;

        struct cairo_paint_result result;
        window->notify.render(&buffer->cairo, buffer->width, window->max_height, menu, &result);
        window->displayed = result.displayed;

        if (window->height == (uint32_t) ceil(result.height / window->scale))
            break;

        window->height = ceil(result.height / window->scale);
        zwlr_layer_surface_v1_set_size(window->layer_surface, window->width, window->height);
        destroy_buffer(buffer);
    }

    assert(ceil(window->width * window->scale) == buffer->width);
    assert(ceil(window->height * window->scale) == buffer->height);

    if (window->wayland->fractional_scaling) {
        assert(window->viewport_surface);
        wp_viewport_set_destination(window->viewport_surface, window->width, window->height);
    } else {
        wl_surface_set_buffer_scale(window->surface, window->scale);
    }

    wl_surface_damage_buffer(window->surface, 0, 0, buffer->width, buffer->height);
    wl_surface_attach(window->surface, buffer->buffer, 0, 0);
    wl_surface_commit(window->surface);
    buffer->busy = true;
    window->render_pending = false;
}

void
bm_wl_window_destroy(struct window *window)
{
    assert(window);

    for (int32_t i = 0; i < 2; ++i)
        destroy_buffer(&window->buffers[i]);

    if (window->layer_surface)
        zwlr_layer_surface_v1_destroy(window->layer_surface);

    if (window->surface)
        wl_surface_destroy(window->surface);
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width, uint32_t height)
{
    struct window *window = data;
    window->width = width;
    window->height = height;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
    struct window *window = data;
    zwlr_layer_surface_v1_destroy(layer_surface);
    wl_surface_destroy(window->surface);
    exit(1);
}

static uint32_t
get_window_width(struct window *window)
{
    uint32_t width = window->width * ((window->width_factor != 0) ? window->width_factor : 1);

    if(width > window->width - 2 * window->hmargin_size)
        width = window->width - 2 * window->hmargin_size;

    if(width < WINDOW_MIN_WIDTH || 2 * window->hmargin_size > window->width)
        width = WINDOW_MIN_WIDTH;

    return width;
}

static void
fractional_scale_preferred_scale(
    void *data, struct wp_fractional_scale_v1 *wp_fractional_scale_v1,
    uint32_t scale)
{
    (void)wp_fractional_scale_v1;
    struct window *window = data;

    window->scale = (double)scale / 120;
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener = {
    .preferred_scale = fractional_scale_preferred_scale,
};

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

void
bm_wl_window_set_width(struct window *window, struct wl_display *display, uint32_t margin, float factor)
{
    if(window->hmargin_size == margin && window->width_factor == factor)
        return;

    window->hmargin_size = margin;
    window->width_factor = factor;

    zwlr_layer_surface_v1_set_anchor(window->layer_surface, window->align_anchor);
    zwlr_layer_surface_v1_set_size(window->layer_surface, get_window_width(window), window->height);

    wl_surface_commit(window->surface);
    wl_display_roundtrip(display);
}

void
bm_wl_window_set_align(struct window *window, struct wl_display *display, enum bm_align align)
{
    if(window->align == align)
        return;

    window->align = align;

    window->align_anchor = get_align_anchor(window->align);

    zwlr_layer_surface_v1_set_anchor(window->layer_surface, window->align_anchor);
    wl_surface_commit(window->surface);
    wl_display_roundtrip(display);
}

void
bm_wl_window_set_y_offset(struct window *window, struct wl_display *display, int32_t y_offset)
{
    if(window->y_offset == y_offset)
        return;

    window->y_offset = y_offset;

    zwlr_layer_surface_v1_set_margin(window->layer_surface, window->y_offset, 0, 0, 0);
    wl_surface_commit(window->surface);
    wl_display_roundtrip(display);
}

void
bm_wl_window_grab_keyboard(struct window *window, struct wl_display *display, bool grab)
{
    zwlr_layer_surface_v1_set_keyboard_interactivity(window->layer_surface, grab);
    wl_surface_commit(window->surface);
    wl_display_roundtrip(display);
}

void
bm_wl_window_set_overlap(struct window *window, struct wl_display *display, bool overlap)
{
    zwlr_layer_surface_v1_set_exclusive_zone(window->layer_surface, overlap ? -1 : 0);
    wl_surface_commit(window->surface);
    wl_display_roundtrip(display);
}

bool
bm_wl_window_create(struct window *window, struct wl_display *display, struct wl_shm *shm, struct wl_output *output, struct zwlr_layer_shell_v1 *layer_shell, struct wl_surface *surface)
{
    assert(window);

    struct wayland *wayland = window->wayland;

    if (wayland->fractional_scaling) {
        assert(wayland->wfs_mgr && wayland->viewporter);

        struct wp_fractional_scale_v1 *wfs_surf = wp_fractional_scale_manager_v1_get_fractional_scale(wayland->wfs_mgr, surface);
        wp_fractional_scale_v1_add_listener(
            wfs_surf, &fractional_scale_listener, window);
        window->viewport_surface = wp_viewporter_get_viewport(wayland->viewporter, surface);
    }

    enum zwlr_layer_shell_v1_layer layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
    if (layer_shell && (window->layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, output, layer, "menu"))) {
        zwlr_layer_surface_v1_add_listener(window->layer_surface, &layer_surface_listener, window);
        window->align_anchor = get_align_anchor(window->align);
        zwlr_layer_surface_v1_set_anchor(window->layer_surface, window->align_anchor);
        zwlr_layer_surface_v1_set_size(window->layer_surface, 0, 32);

        wl_surface_commit(surface);
        wl_display_roundtrip(display);

        zwlr_layer_surface_v1_set_size(window->layer_surface, get_window_width(window), 32);
    } else {
        return false;
    }

    window->shm = shm;
    window->surface = surface;
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
