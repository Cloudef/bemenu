#include "internal.h"
#include "wayland.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

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
create_buffer(struct wl_shm *shm, struct buffer *buffer, int32_t width, int32_t height, uint32_t format)
{
    int fd = -1;
    struct wl_shm_pool *pool = NULL;
    uint32_t stride = width * 4;
    uint32_t size = stride * height;

    if ((fd = os_create_anonymous_file(size)) < 0) {
        fprintf(stderr, "creating a buffer file for %d B failed: %m\n", size);
        return false;
    }

    void *data;
    if ((data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %m\n");
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

    if (window->width != buffer->width || window->height != buffer->height)
        destroy_buffer(buffer);

    if (!buffer->buffer && !create_buffer(window->shm, buffer, window->width, window->height, WL_SHM_FORMAT_ARGB8888))
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
}

static const struct wl_callback_listener listener = {
    frame_callback
};

void
bm_wl_window_render(struct window *window, struct wl_display *display, const struct bm_menu *menu)
{
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
        window->notify.render(&buffer->cairo, buffer->width, fmin(buffer->height, window->max_height), window->max_height, menu, &result);
        window->displayed = result.displayed;

        if (window->height == result.height)
            break;

        window->height = result.height;
        zwlr_layer_surface_v1_set_size(window->layer_surface, 0, window->height);
        wl_surface_commit(window->surface);
        wl_display_roundtrip(display);
        destroy_buffer(buffer);
    }

    window->frame_cb = wl_surface_frame(window->surface);
    wl_callback_add_listener(window->frame_cb, &listener, window);

    wl_surface_damage(window->surface, 0, 0, buffer->width, buffer->height);
    wl_surface_attach(window->surface, buffer->buffer, 0, 0);
    wl_surface_commit(window->surface);
    buffer->busy = true;
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

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

void
bm_wl_window_set_bottom(struct window *window, struct wl_display *display, bool bottom)
{
    if (window->bottom == bottom)
        return;

    window->bottom = bottom;

    zwlr_layer_surface_v1_set_anchor(window->layer_surface, (window->bottom ? ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM : ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
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

bool
bm_wl_window_create(struct window *window, struct wl_display *display, struct wl_shm *shm, struct wl_output *output, struct zwlr_layer_shell_v1 *layer_shell, struct wl_surface *surface)
{
    assert(window);

    if (layer_shell && (window->layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, output, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "menu"))) {
        zwlr_layer_surface_v1_add_listener(window->layer_surface, &layer_surface_listener, window);
        zwlr_layer_surface_v1_set_exclusive_zone(window->layer_surface, -1);
        zwlr_layer_surface_v1_set_anchor(window->layer_surface, (window->bottom ? ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM : ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_size(window->layer_surface, 0, 32);
        wl_surface_commit(surface);
        wl_display_roundtrip(display);
    } else {
        return false;
    }

    window->shm = shm;
    window->surface = surface;
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
