#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common/common.h"

static struct client client = {
    .filter_mode = BM_FILTER_MODE_DMENU,
    .title = "bemenu",
};

static void
read_items_to_menu_from_stdin(struct bm_menu *menu)
{
    assert(menu);

    size_t step = 1024, allocated;
    char *buffer;

    if (!(buffer = malloc((allocated = step))))
        return;

    // TODO: do not read everything into memory
    size_t read;
    while ((read = fread(buffer + (allocated - step), 1, step, stdin)) == step) {
        void *tmp;
        if (!(tmp = realloc(buffer, (allocated += step)))) {
            free(buffer);
            fprintf(stderr, "Out of memory\n");
            return;
        }
        buffer = tmp;
    }

    // make sure we can null terminate the input
    const size_t end = allocated - step + read;
    assert(end != ~(size_t)0);
    if (end >= allocated && !(buffer = realloc(buffer, end + 1))) {
        fprintf(stderr, "Out of memory\n");
        return;
    }

    buffer[end] = 0;

    char *s = buffer;
    while ((size_t)(s - buffer) < end && *s != 0) {
        const size_t pos = strcspn(s, "\n");
        s[pos] = 0;

        struct bm_item *item;
        if (!(item = bm_item_new(s)))
            break;

        bm_menu_add_item(menu, item);
        s += pos + 1;
    }

    free(buffer);
}

static void
item_cb(const struct client *client, struct bm_item *item)
{
    (void)client;
    const char *text = bm_item_get_text(item);
    printf("%s\n", (text ? text : ""));
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

    read_items_to_menu_from_stdin(menu);
    const enum bm_run_result status = run_menu(&client, menu, item_cb);
    bm_menu_free(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
