#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include "common.h"
#include "../lib/3rdparty/tinydir.h"

static struct client client = {
    .filter_mode = BM_FILTER_MODE_DMENU,
    .colors = {0},
    .title = "bemenu",
    .prefix = NULL,
    .font = NULL,
    .lines = 0,
    .selected = 0,
    .monitor = 0,
    .bottom = false,
    .grab = false,
    .wrap = false,
};

struct paths {
   char *path;
   char *paths;
};

static char*
c_strdup(const char *str)
{
    size_t size = strlen(str);
    char *cpy = calloc(1, size + 1);
    return (cpy ? memcpy(cpy, str, size) : NULL);
}

    static char*
strip_slash(char *str)
{
    size_t size = strlen(str);
    if (size > 0)
        for (char *s = str + size - 1; *s == '/'; --s)
            *s = 0;
    return str;
}

static const char*
get_paths(const char *env, const char *default_paths, struct paths *state)
{
    if (state->path && !*state->path) {
        free(state->paths);
        return NULL;
    }

    if (!state->paths) {
        const char *paths;
        if (!(paths = getenv(env)) || !paths[0])
            paths = default_paths;

        state->path = state->paths = c_strdup(paths);
    }

    if (!state->path || !state->paths)
        return NULL;

    char *path;
    do {
        size_t f;
        path = state->path;
        if ((f = strcspn(state->path, ":")) > 0) {
            state->path += f + (path[f] ? 1 : 0);
            path[f] = 0;
        }

        if (!*path) {
            free(state->paths);
            return NULL;
        }
    } while (path[0] != '/');

    return strip_slash(path);
}

static int
compare(const void *a, const void *b)
{
    const struct bm_item *ia = *(struct bm_item**)a, *ib = *(struct bm_item**)b;
    return strcmp(bm_item_get_text(ia), bm_item_get_text(ib));
}

static void
read_items_to_menu_from_dir(struct bm_menu *menu, const char *path)
{
    assert(menu && path);

    tinydir_dir dir;
    if (tinydir_open(&dir, path) == -1)
        return;

    while (dir.has_next) {
        tinydir_file file;
        memset(&file, 0, sizeof(file));
        tinydir_readfile(&dir, &file);

        if (!file.is_dir && file.name) {
            struct bm_item *item;
            if (!(item = bm_item_new(file.name)))
                break;

            bm_menu_add_item(menu, item);
        }

        tinydir_next(&dir);
    }

    tinydir_close(&dir);

    uint32_t count;
    struct bm_item **items = bm_menu_get_items(menu, &count);
    qsort(items, count, sizeof(struct bm_item*), compare);

    bool unique = true;
    for (uint32_t i = 0; i + 1 < count; i += unique) {
        if (!(unique = strcmp(bm_item_get_text(items[i]), bm_item_get_text(items[i + 1])))) {
            bm_item_free(items[i]);
            bm_menu_remove_item_at(menu, i);
            items = bm_menu_get_items(menu, &count);
        }
    }
}

static void
read_items_to_menu_from_path(struct bm_menu *menu)
{
    assert(menu);

    const char *path;
    struct paths state;
    memset(&state, 0, sizeof(state));
    while ((path = get_paths("PATH", "/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:/bin:/sbin", &state)))
        read_items_to_menu_from_dir(menu, path);
}

static bool
tokenize_args(const char *buf, char **out_copy, char ***out_list)
{
    assert(buf && out_copy && out_list);
    *out_copy = NULL;
    *out_list = NULL;

    char *copy;
    if (!(copy = c_strdup(buf)))
        return false;

    size_t pos, count = 0;
    for (const char *s = copy; *s && (pos = strcspn(s, " ")) != 0; s += pos + 1)
        ++count;

    char **list;
    if (!(list = calloc(count + 1, sizeof(char*)))) {
        free(copy);
        return false;
    }

    count = 0;
    for (char *s = copy; *s && (pos = strcspn(s, " ")) != 0; s += pos + 1, ++count) {
        s[pos] = 0;
        list[count] = s;
    }

    *out_copy = copy;
    *out_list = list;
    return true;
}

static void
launch(const char *bin)
{
    if (!bin)
        return;

    if (fork() == 0) {
        setsid();
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

        char *first, **args;
        if (!tokenize_args(bin, &first, &args))
            _exit(EXIT_FAILURE);

        execvp(first, args);
        free(first);
        free(args);
        _exit(EXIT_SUCCESS);
    }
}

int
main(int argc, char **argv)
{
    struct sigaction action = {
        .sa_handler = SIG_DFL,
        .sa_flags = SA_NOCLDWAIT
    };

    // do not care about childs
    sigaction(SIGCHLD, &action, NULL);

    if (!bm_init())
        return EXIT_FAILURE;

    parse_args(&client, &argc, &argv);

    struct bm_menu *menu;
    if (!(menu = menu_with_options(&client)))
        return EXIT_FAILURE;

    read_items_to_menu_from_path(menu);
    bm_menu_set_highlighted_index(menu, client.selected);

    enum bm_run_result status = run_menu(menu);

    if (status == BM_RUN_RESULT_SELECTED) {
        uint32_t i, count;
        struct bm_item **items = bm_menu_get_selected_items(menu, &count);
        for (i = 0; i < count; ++i) {
            const char *text = bm_item_get_text(items[i]);
            launch(text);
        }

        if (!count && bm_menu_get_filter(menu))
            launch(bm_menu_get_filter(menu));
    }

    bm_menu_free(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
