/**
 * @file client.c
 *
 * Sample client using the libbemenu.
 * Also usable as dmenu replacement.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <bemenu.h>

static ptrdiff_t getLine(char **outLine, size_t *outAllocated, FILE *stream)
{
    size_t len = 0, allocated;
    char *s, *buffer;

    assert(outLine);
    assert(outAllocated);

    if (!stream || feof(stream) || ferror(stream))
        return -1;

    allocated = *outAllocated;
    buffer = *outLine;

    if (!buffer || allocated == 0) {
        if (!(buffer = calloc(1, (allocated = 1024) + 1)))
            return -1;
    }

    for (s = buffer;;) {
        if (!fgets(s, allocated - (s - buffer), stream)) {
            *outAllocated = allocated;
            *outLine = buffer;
            return -1;
        }

        len = strlen(s);
        if (feof(stream))
            break;

        if (len > 0 && s[len - 1] == '\n')
            break;

        if (len + 1 >= allocated - (s - buffer)) {
            void *tmp = realloc(buffer, 2 * allocated);
            if (!tmp)
                break;

            buffer = tmp;
            s = buffer + allocated - 1;
            memset(s, 0, allocated - (s - buffer));
            allocated *= 2;
        } else {
            s += len;
        }
    }

    *outAllocated = allocated;
    *outLine = buffer;

    if (s[len - 1] == '\n')
        s[len - 1] = 0;

    return s - buffer + len;
}

static void readItemsToMenuFromStdin(bmMenu *menu)
{
    ptrdiff_t len;
    size_t size = 0;
    char *line = NULL;

    while ((len = getLine(&line, &size, stdin)) != -1) {
        bmItem *item = bmItemNew((len > 0 ? line : NULL));
        if (!item)
            break;

        bmMenuAddItem(menu, item);
    }

    if (line)
        free(line);
}

/**
 * Main method
 *
 * This function gives and takes the life of our program.
 *
 * @param argc Number of arguments from command line
 * @param argv Pointer to array of the arguments
 * @return exit status of the program
 */
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
        bmItem *item = bmMenuGetSelectedItem(menu);

        if (item)
            printf("%s\n", bmItemGetText(item));
    }

    bmMenuFree(menu);

    return (status == BM_RUN_RESULT_SELECTED ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set ts=8 sw=4 tw=0 :*/
