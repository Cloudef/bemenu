#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common/common.h"

static struct client client = {
    .filter_mode = BM_FILTER_MODE_DMENU,
    .title = "bemenu",
    .monitor = -1,
};

static void
read_items_to_menu_from_stdin(struct bm_menu *menu)
{
    assert(menu);

    ssize_t n;
    size_t llen = 0;
    char *line = NULL;

    while ((n = getline(&line, &llen, stdin)) > 0) {
        // Remove trailing newline (if any)
        if (line[n - 1] == '\n')
            line[n - 1] = '\0';

        struct bm_item *item;
        if (!(item = bm_item_new(line)))
            break;

        bm_menu_add_item(menu, item);
    }
    free(line);

    if (ferror(stdin)) {
        fprintf(stderr, "getline failed");
        return;
    }
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
    switch (status) {
        case BM_RUN_RESULT_SELECTED:
            return EXIT_SUCCESS;

        case BM_RUN_RESULT_CUSTOM_1: return 10;
        case BM_RUN_RESULT_CUSTOM_2: return 11;
        case BM_RUN_RESULT_CUSTOM_3: return 12;
        case BM_RUN_RESULT_CUSTOM_4: return 13;
        case BM_RUN_RESULT_CUSTOM_5: return 14;
        case BM_RUN_RESULT_CUSTOM_6: return 15;
        case BM_RUN_RESULT_CUSTOM_7: return 16;
        case BM_RUN_RESULT_CUSTOM_8: return 17;
        case BM_RUN_RESULT_CUSTOM_9: return 18;
        case BM_RUN_RESULT_CUSTOM_10: return 19;

        default:
            return EXIT_FAILURE;
    }
}

/* vim: set ts=8 sw=4 tw=0 :*/
