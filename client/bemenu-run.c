#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include "common/common.h"

static struct client client = {
    .filter_mode = BM_FILTER_MODE_DMENU,
    .title = "bemenu-run",
    .monitor = -1,
};

struct paths {
   char *path;
   char *paths;
};

static char*
c_strdup(const char *str)
{
    return cstrcopy(str, strlen(str));
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
        } else {
            state->path += 1;
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

    DIR *dir;
    if (!(dir = opendir(path)))
        return;

    struct dirent *file;
    while ((file = readdir(dir))) {
        if (file->d_type != DT_DIR && strlen(file->d_name) && file->d_name[0] != '.') {
            struct bm_item *item;
            if (!(item = bm_item_new(file->d_name)))
                break;

            bm_menu_add_item(menu, item);
        }
    }

    closedir(dir);

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

static inline void ignore_ret(int useless, ...) { (void)useless; }

static void
launch(const struct client *client, const char *bin)
{
    struct sigaction action = {
        .sa_handler = SIG_DFL,
        .sa_flags = SA_NOCLDWAIT
    };

    // do not care about childs
    sigaction(SIGCHLD, &action, NULL);

    if (!bin)
        return;

    if (!client->fork || fork() == 0) {
        if (client->fork) {
            setsid();
            ignore_ret(0, freopen("/dev/null", "w", stdout));
            ignore_ret(0, freopen("/dev/null", "w", stderr));
        }

        char **tokens;
        if (!(tokens = tokenize_quoted_to_argv(bin, NULL, NULL)))
            _exit(EXIT_FAILURE);

        execvp(tokens[0], tokens);
        _exit(EXIT_SUCCESS);
    }
}

static void
item_cb(const struct client *client, struct bm_item *item)
{
    if (client->no_exec) {
        const char *text = bm_item_get_text(item);
        printf("%s\n", (text ? text : ""));
    } else {
        launch(client, bm_item_get_text(item));
    }
}

int
main(int argc, char **argv)
{
    if (!bm_init())
        return EXIT_FAILURE;

    parse_args(&client, &argc, &argv);

    struct bm_menu *menu;
    if (!(menu = menu_with_options(&client)))
        return EXIT_FAILURE;

    read_items_to_menu_from_path(menu);
    const enum bm_run_result status = run_menu(&client, menu, item_cb);
    bm_menu_free(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
