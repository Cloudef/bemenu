#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <bemenu.h>
#include <getopt.h>

static struct {
    bmFilterMode filterMode;
    int wrap;
    unsigned int lines;
    const char *title;
    int selected;
    int bottom;
    int grab;
    int monitor;
} client = {
    BM_FILTER_MODE_DMENU, /* filterMode */
    0, /* wrap */
    0, /* lines */
    "bemenu", /* title */
    0, /* selected */
    0, /* bottom */
    0, /* grab */
    0 /* monitor */
};

static void version(const char *name)
{
    char *base = strrchr(name, '/');
    printf("%s v%s\n", (base ? base  + 1 : name), bmVersion());
    exit(EXIT_SUCCESS);
}

static void usage(FILE *out, const char *name)
{
    char *base = strrchr(name, '/');
    fprintf(out, "usage: %s [options]\n", (base ? base + 1 : name));
    fputs("Options\n"
          " -h, --help            display this help and exit.\n"
          " -v, --version         display version.\n"
          " -i, --ignorecase      match items case insensitively.\n"
          " -w, --wrap            wraps cursor selection.\n"
          " -l, --list            list items vertically with the given number of lines.\n"
          " -p, --prompt          defines the prompt text to be displayed.\n"
          " -I, --index           select item at index automatically.\n\n"

          "Backend specific options\n"
          "   c = ncurses\n" // x == x11
          "   (...) At end of help indicates the backend support for option.\n\n"

          " -b, --bottom          appears at the bottom of the screen. ()\n"
          " -f, --grab            grabs the keyboard before reading stdin. ()\n"
          " -m, --monitor         index of monitor where menu will appear. ()\n"
          " -fn, --fn             defines the font to be used. ()\n"
          " -nb, --nb             defines the normal background color. ()\n"
          " -nf, --nf             defines the normal foreground color. ()\n"
          " -sb, --sb             defines the selected background color. ()\n"
          " -sf, --sf             defines the selected foreground color. ()\n", out);
    exit((out == stderr ? EXIT_FAILURE : EXIT_SUCCESS));
}

static void parseArgs(int *argc, char **argv[])
{
    static const struct option opts[] = {
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },

        { "ignorecase",  no_argument,       0, 'i' },
        { "wrap",        no_argument,       0, 'w' },
        { "list",        required_argument, 0, 'l' },
        { "prompt",      required_argument, 0, 'p' },
        { "index",       required_argument, 0, 'I' },

        { "bottom",      no_argument,       0, 'b' },
        { "grab",        no_argument,       0, 'f' },
        { "monitor",     required_argument, 0, 'm' },
        { "fn",          required_argument, 0, 0x100 },
        { "nb",          required_argument, 0, 0x101 },
        { "nf",          required_argument, 0, 0x102 },
        { "sb",          required_argument, 0, 0x103 },
        { "sf",          required_argument, 0, 0x104 },
        { 0, 0, 0, 0 }
    };

    /* TODO: getopt does not support -sf, -sb etc..
     * Either break the interface and make them --sf, --sb (like they are now),
     * or parse them before running getopt.. */

    for (;;) {
        int opt = getopt_long(*argc, *argv, "hviwl:I:p:I:bfm:", opts, NULL);
        if (opt < 0)
            break;

        switch (opt) {
            case 'h':
                usage(stdout, *argv[0]);
                break;
            case 'v':
                version(*argv[0]);
                break;

            case 'i':
                client.filterMode = BM_FILTER_MODE_DMENU_CASE_INSENSITIVE;
                break;
            case 'w':
                client.wrap = 1;
                break;
            case 'l':
                client.lines = strtol(optarg, NULL, 10);
                break;
            case 'p':
                client.title = optarg;
                break;
            case 'I':
                client.selected = strtol(optarg, NULL, 10);
                break;

            case 'b':
                client.bottom = 1;
                break;
            case 'f':
                client.grab = 1;
                break;
            case 'm':
                client.monitor = strtol(optarg, NULL, 10);
                break;

            case 0x100:
            case 0x101:
            case 0x102:
            case 0x103:
            case 0x104:
                break;

            case ':':
            case '?':
                fputs("\n", stderr);
                usage(stderr, *argv[0]);
                break;
        }
    }

    *argc -= optind;
    *argv += optind;
}

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

    size_t pos;
    char *s = buffer;
    while ((pos = strcspn(s, "\n")) != 0) {
        size_t next = pos + (s[pos] != 0);
        s[pos] = 0;

        bmItem *item = bmItemNew(s);
        if (!item)
            break;

        bmMenuAddItem(menu, item);
        s += next;
    }

    free(buffer);
}

int main(int argc, char **argv)
{
    parseArgs(&argc, &argv);

    bmMenu *menu = bmMenuNew(BM_DRAW_MODE_CURSES);
    if (!menu)
        return EXIT_FAILURE;

    bmMenuSetTitle(menu, client.title);
    bmMenuSetFilterMode(menu, client.filterMode);
    bmMenuSetWrap(menu, client.wrap);

    readItemsToMenuFromStdin(menu);

    bmMenuSetHighlightedIndex(menu, client.selected);

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
