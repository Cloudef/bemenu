#include "internal.h"
#include "version.h"

#include "3rdparty/tinydir.h"
#include "3rdparty/cdl.h"

#include <unistd.h>
#include <stdio.h>
#include <assert.h>

static struct list renderers;

static void
bm_renderer_free(struct bm_renderer *renderer)
{
    assert(renderer);

    if (renderer->handle)
        chckDlUnload(renderer->handle);

    free(renderer->name);
    free(renderer->file);
    free(renderer);
}

static bool
load(const char *file, struct bm_renderer *renderer)
{
    void *handle;
    const char *error = NULL;

    if (!(handle = chckDlLoad(file, &error)))
        goto load_fail;

    const char* (*regfun)(struct render_api*);
    if (!(regfun = chckDlLoadSymbol(handle, "register_renderer", &error)))
        goto load_fail;

    const char *name;
    if (!(name = regfun(&renderer->api)))
        goto fail;

    if (strcmp(renderer->api.version, BM_PLUGIN_VERSION))
        goto mismatch_fail;

    if (!renderer->name)
        renderer->name = bm_strdup(name);

    if (!renderer->file)
        renderer->file = bm_strdup(file);

    renderer->handle = handle;
    return true;

load_fail:
    fprintf(stderr, "%s\n", error);
    goto fail;
mismatch_fail:
    fprintf(stderr, "%s: version mismatch (%s != %s)\n", name, renderer->api.version, BM_PLUGIN_VERSION);
fail:
    if (handle)
        chckDlUnload(handle);
    return false;
}

static int
compare(const void *a, const void *b)
{
    const struct bm_renderer *ra = *(struct bm_renderer**)a, *rb = *(struct bm_renderer**)b;
    return (ra->api.prioritory > rb->api.prioritory);
}

static bool
load_to_list(const char *file)
{
    struct bm_renderer *renderer;
    if (!(renderer = calloc(1, sizeof(struct bm_renderer))))
        goto fail;

    if (!load(file, renderer))
        goto fail;

    chckDlUnload(renderer->handle);
    renderer->handle = NULL;

    if (!list_add_item(&renderers, renderer))
        goto fail;

    list_sort(&renderers, compare);
    return true;

fail:
    if (renderer)
        bm_renderer_free(renderer);
    return false;
}

bool
bm_renderer_activate(struct bm_renderer *renderer, struct bm_menu *menu)
{
    assert(renderer);

    if (!load(renderer->file, renderer))
        return false;

    menu->renderer = renderer;

    if (!renderer->api.constructor(menu))
        goto fail;

    return true;

fail:
    chckDlUnload(renderer->handle);
    menu->renderer = NULL;
    return false;
}

bool
bm_init(void)
{
    if (renderers.count > 0)
        return true;

    static const char *rpath = INSTALL_PREFIX "/lib/bemenu";
    const char *path = getenv("BEMENU_RENDERER");

    if (path)
        return load_to_list(path);

    path = getenv("BEMENU_RENDERERS");

    if (!path || access(path, R_OK) == -1)
        path = rpath;

    tinydir_dir dir;
    if (tinydir_open(&dir, path) == -1)
        goto fail;

    while (dir.has_next) {
        tinydir_file file;
        memset(&file, 0, sizeof(file));
        tinydir_readfile(&dir, &file);

        if (!file.is_dir && !strncmp(file.name, "bemenu-renderer-", strlen("bemenu-renderer-"))) {
            char *fpath;
            if ((fpath = bm_dprintf("%s/%s", path, file.name))) {
                load_to_list(fpath);
                free(fpath);
            }
        }

        tinydir_next(&dir);
    }

    tinydir_close(&dir);
    return (renderers.count > 0 ? true : false);

fail:
    return false;
}

const struct bm_renderer**
bm_get_renderers(uint32_t *out_nmemb)
{
    assert(out_nmemb);
    return list_get_items(&renderers, out_nmemb);
}

const char*
bm_version(void)
{
    return BM_VERSION;
}

const char*
bm_renderer_get_name(const struct bm_renderer *renderer)
{
    assert(renderer);
    return renderer->name;
}

enum bm_prioritory
bm_renderer_get_prioritory(const struct bm_renderer *renderer)
{
    assert(renderer);
    return renderer->api.prioritory;
}

/* vim: set ts=8 sw=4 tw=0 :*/
