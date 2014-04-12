#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <bemenu.h>

static void readItemsToMenuFromStdin(bmMenu *menu)
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

    char *s;
    for (s = strtok(buffer, "\n"); s; s = strtok(NULL, "\n")) {
        bmItem *item = bmItemNew(s);
        if (!item)
            break;

        bmMenuAddItem(menu, item);
    }

    free(buffer);
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    bmMenu *menu = bmMenuNew(BM_DRAW_MODE_CURSES);
    if (!menu)
        return EXIT_FAILURE;

    bmMenuSetTitle(menu, "bemenu");
    readItemsToMenuFromStdin(menu);

    bmKey key;
    unsigned int unicode;
    int status = 0;
    do {
        bmMenuRender(menu);
        key = bmMenuGetKey(menu, &unicode);
    } while ((status = bmMenuRunWithKey(menu, key, unicode)) == BM_RUN_RESULT_RUNNING);

    if (status == BM_RUN_RESULT_SELECTED) {
        unsigned int i, count;
        bmItem **items = bmMenuGetSelectedItems(menu, &count);
        for (i = 0; i < count; ++i) {
            const char *text = bmItemGetText(items[i]);
            printf("%s\n", (text ? text : ""));
        }

        if (!count && bmMenuGetFilter(menu))
            printf("%s\n", bmMenuGetFilter(menu));
    }

    bmMenuFree(menu);
    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
