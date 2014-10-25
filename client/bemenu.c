#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common.h"

static struct client client = {
    .prioritory = BM_PRIO_ANY,
    .filter_mode = BM_FILTER_MODE_DMENU,
    .wrap = 0,
    .lines = 0,
    .colors = {0},
    .title = "bemenu",
    .renderer = NULL,
    .font = NULL,
    .font_size = 0,
    .selected = 0,
    .bottom = 0,
    .grab = 0,
    .monitor = 0
};

static void
read_items_to_menu_from_stdin(struct bm_menu *menu)
{
    assert(menu);

    size_t step = 1024, allocated;
    char *buffer;

    if (!(buffer = malloc((allocated = step))))
        return;

    size_t read;
    while ((read = fread(buffer + (allocated - step), 1, step, stdin)) == step) {
        void *tmp;
        if (!(tmp = realloc(buffer, (allocated += step)))) {
            free(buffer);
            return;
        }
        buffer = tmp;
    }
    buffer[allocated - step + read - 1] = 0;

    char *s = buffer;
    while ((size_t)(s - buffer) < allocated - step + read) {
        size_t pos = strcspn(s, "\n");
        if (pos == 0) {
            s += 1;
            continue;
        }

        s[pos] = 0;

        struct bm_item *item;
        if (!(item = bm_item_new(s)))
            break;

        bm_menu_add_item(menu, item);
        s += pos + 1;
    }

    free(buffer);
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
    bm_menu_set_highlighted_index(menu, client.selected);

    enum bm_run_result status = run_menu(menu);

    if (status == BM_RUN_RESULT_SELECTED) {
        uint32_t i, count;
        struct bm_item **items = bm_menu_get_selected_items(menu, &count);
        for (i = 0; i < count; ++i) {
            const char *text = bm_item_get_text(items[i]);
            printf("%s\n", (text ? text : ""));
        }

        if (!count && bm_menu_get_filter(menu))
            printf("%s\n", bm_menu_get_filter(menu));
    }

    free(client.font);
    bm_menu_free(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
