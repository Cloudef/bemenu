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
        goto fail;

    const char* (*regfun)(struct render_api*);
    if (!(regfun = chckDlLoadSymbol(handle, "register_renderer", &error)))
        goto fail;

    renderer->file = bm_strdup(file);
    renderer->handle = handle;
    const char *name = regfun(&renderer->api);
    renderer->name = (name ? bm_strdup(name) : NULL);
    return true;

fail:
    if (handle)
        chckDlUnload(handle);
    fprintf(stderr, "%s\n", error);
    return false;
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
    return list_add_item(&renderers, renderer);

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

    if (!renderer->api.constructor(menu))
        goto fail;

    return true;

fail:
    chckDlUnload(renderer->handle);
    return false;
}

bool
bm_init(void)
{
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

    size_t registered = 0;
    while (dir.has_next) {
        tinydir_file file;
        memset(&file, 0, sizeof(file));
        tinydir_readfile(&dir, &file);

        if (!file.is_dir && !strncmp(file.name, "bemenu-renderer-", strlen("bemenu-renderer-"))) {
            size_t len = snprintf(NULL, 0, "%s/%s", file.name, path);

            char *fpath;
            if ((fpath = calloc(1, len + 1))) {
                sprintf(fpath, "%s/%s", path, file.name);

                if (load_to_list(fpath))
                    registered++;

                free(fpath);
            }
        }

        tinydir_next(&dir);
    }

    tinydir_close(&dir);
    return (registered > 0 ? true : false);

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

/* vim: set ts=8 sw=4 tw=0 :*/
