#define _DEFAULT_SOURCE
#include "internal.h"
#include "wayland.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

#define USE_XDG_SHELL false

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
    if (buffer->cairo.cr)
        cairo_destroy(buffer->cairo.cr);
    if (buffer->cairo.surface)
        cairo_surface_destroy(buffer->cairo.surface);

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

    if (!(buffer->cairo.surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride)))
        goto fail;

    if (!(buffer->cairo.cr = cairo_create(buffer->cairo.surface)))
        goto fail;

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
resize(struct window *window, uint32_t width, uint32_t height)
{
    window->width = width;
    window->height = height;
}

static void
shell_surface_ping(void *data, struct wl_shell_surface *surface, uint32_t serial)
{
    (void)data;
   wl_shell_surface_pong(surface, serial);
}

static void
shell_surface_configure(void *data, struct wl_shell_surface *surface, uint32_t edges, int32_t width, int32_t height)
{
    (void)surface, (void)edges;
    resize(data, width, height);
}

static void
shell_surface_popup_done(void *data, struct wl_shell_surface *surface)
{
    (void)data, (void)surface;
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    .ping = shell_surface_ping,
    .configure = shell_surface_configure,
    .popup_done = shell_surface_popup_done,
};

static void
xdg_surface_configure(void *data, struct xdg_surface *surface, int32_t width, int32_t height, struct wl_array *states, uint32_t serial)
{
    (void)states;
    resize(data, width, height);
    xdg_surface_ack_configure(surface, serial);
}

static void
xdg_surface_close(void *data, struct xdg_surface *surface)
{
    (void)data, (void)surface;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
    .close = xdg_surface_close,
};

void
bm_wl_window_render(struct window *window, const struct bm_menu *menu, uint32_t lines)
{
    assert(window && menu);

    struct buffer *buffer;
    if (!(buffer = next_buffer(window)))
        return;

    cairo_font_extents_t fe;
    bm_cairo_get_font_extents(&buffer->cairo, &menu->font, &fe);
    window->height = lines * (fe.height + 4);

    if (window->height != buffer->height && !(buffer = next_buffer(window)))
        return;

    if (window->notify.render)
        window->displayed = window->notify.render(&buffer->cairo, buffer->width, buffer->height, menu);

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

    if (window->xdg_surface)
        xdg_surface_destroy(window->xdg_surface);

    if (window->shell_surface)
        wl_shell_surface_destroy(window->shell_surface);

    if (window->surface)
        wl_surface_destroy(window->surface);
}

bool
bm_wl_window_create(struct window *window, struct wl_shm *shm, struct wl_shell *shell, struct xdg_shell *xdg_shell, struct wl_surface *surface)
{
    assert(window);

    if (USE_XDG_SHELL && xdg_shell && (window->xdg_surface = xdg_shell_get_xdg_surface(xdg_shell, surface))) {
        xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
        xdg_surface_set_title(window->xdg_surface, "bemenu");
    } else if (shell && (window->shell_surface = wl_shell_get_shell_surface(shell, surface))) {
        wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, window);
        wl_shell_surface_set_title(window->shell_surface, "bemenu");
        wl_shell_surface_set_class(window->shell_surface, "bemenu");
        wl_shell_surface_set_toplevel(window->shell_surface);
    } else {
        return false;
    }

    window->width = 800;
    window->height = 240;

    window->shm = shm;
    window->surface = surface;
    wl_surface_damage(surface, 0, 0, window->width, window->height);
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
