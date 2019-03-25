#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

static void
disco_trap(int sig)
{
    (void)sig;
    fprintf(stderr, "\e[?25h");
    fflush(stderr);
    exit(EXIT_FAILURE);
}

static void
disco(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = disco_trap;
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGTRAP, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    uint32_t cc, c = 80;
    fprintf(stderr, "\e[?25l");
    while (1) {
        for (uint32_t i = 1; i < c - 1; ++i) {
            fprintf(stderr, "\r    %*s%s %s %s ", (i > c / 2 ? c - i : i), " ", ((i % 2) ? "<o/" : "\\o>"), ((i % 4) ? "DISCO" : "     "), ((i %2) ? "\\o>" : "<o/"));
            for (cc = 0; cc < (i < c / 2 ? c / 2 - i : i - c / 2); ++cc) fprintf(stderr, ((i % 2) ? "^" : "'"));
            fprintf(stderr, "%s %s \r %s %s", ((i % 2) ? "*" : "•"), ((i % 3) ? "\\o" : "<o"), ((i % 3) ? "o/" : "o>"), ((i % 2) ? "*" : "•"));
            for (cc = 2; cc < (i > c / 2 ? c - i : i); ++cc) fprintf(stderr, ((i % 2) ? "^" : "'"));
            fflush(stderr);
            usleep(140 * 1000);
        }
    }
    fprintf(stderr, "\e[?25h");
    exit(EXIT_SUCCESS);
}

static void
version(const char *name)
{
    assert(name);
    char *base = strrchr(name, '/');
    printf("%s v%s\n", (base ? base  + 1 : name), bm_version());
    exit(EXIT_SUCCESS);
}

static void
usage(FILE *out, const char *name)
{
    assert(out && name);

    char *base = strrchr(name, '/');
    fprintf(out, "usage: %s [options]\n", (base ? base + 1 : name));
    fputs("Options\n"
          " -h, --help            display this help and exit.\n"
          " -v, --version         display version.\n"
          " -i, --ignorecase      match items case insensitively.\n"
          " -w, --wrap            wraps cursor selection.\n"
          " -l, --list            list items vertically with the given number of lines.\n"
          " -p, --prompt          defines the prompt text to be displayed.\n"
          " -P, --prefix          text to shown before highlighted item.\n"
          " -I, --index           select item at index automatically.\n"
          " --scrollbar           display scrollbar. (always, autohide)\n"
          " --ifne                only display menu if there are items.\n\n"

          "Use BEMENU_BACKEND env variable to force backend:\n"
          " curses               ncurses based terminal backend\n"
          " wayland              wayland backend\n"
          " x11                  x11 backend\n\n"

          "If BEMENU_BACKEND is empty, one of the GUI backends is selected automatically.\n\n"

          "Backend specific options\n"
          "   c = ncurses, w == wayland, x == x11\n"
          "   (...) At end of help indicates the backend support for option.\n\n"

          " -b, --bottom          appears at the bottom of the screen. (wx)\n"
          " -f, --grab            show the menu before reading stdin. (wx)\n"
          " -m, --monitor         index of monitor where menu will appear. (x)\n"
          " -n, --no-overlap      adjust geometry to not overlap with panels. (w)\n"
          " --fn                  defines the font to be used ('name [size]'). (wx)\n"
          " --tb                  defines the title background color. (wx)\n"
          " --tf                  defines the title foreground color. (wx)\n"
          " --fb                  defines the filter background color. (wx)\n"
          " --ff                  defines the filter foreground color. (wx)\n"
          " --nb                  defines the normal background color. (wx)\n"
          " --nf                  defines the normal foreground color. (wx)\n"
          " --hb                  defines the highlighted background color. (wx)\n"
          " --hf                  defines the highlighted foreground color. (wx)\n"
          " --sb                  defines the selected background color. (wx)\n"
          " --sf                  defines the selected foreground color. (wx)\n"
          " --scb                 defines the scrollbar background color. (wx)\n"
          " --scf                 defines the scrollbar foreground color. (wx)\n", out);

    exit((out == stderr ? EXIT_FAILURE : EXIT_SUCCESS));
}

void
parse_args(struct client *client, int *argc, char **argv[])
{
    assert(client && argc && argv);

    static const struct option opts[] = {
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },

        { "ignorecase",  no_argument,       0, 'i' },
        { "wrap",        no_argument,       0, 'w' },
        { "list",        required_argument, 0, 'l' },
        { "prompt",      required_argument, 0, 'p' },
        { "index",       required_argument, 0, 'I' },
        { "prefix",      required_argument, 0, 'P' },
        { "scrollbar",   required_argument, 0, 0x100 },
        { "ifne",        no_argument,       0, 0x115 },

        { "bottom",      no_argument,       0, 'b' },
        { "grab",        no_argument,       0, 'f' },
        { "monitor",     required_argument, 0, 'm' },
        { "fn",          required_argument, 0, 0x101 },
        { "tb",          required_argument, 0, 0x102 },
        { "tf",          required_argument, 0, 0x103 },
        { "fb",          required_argument, 0, 0x104 },
        { "ff",          required_argument, 0, 0x105 },
        { "nb",          required_argument, 0, 0x106 },
        { "nf",          required_argument, 0, 0x107 },
        { "hb",          required_argument, 0, 0x108 },
        { "hf",          required_argument, 0, 0x109 },
        { "sb",          required_argument, 0, 0x110 },
        { "sf",          required_argument, 0, 0x111 },
        { "scb",         required_argument, 0, 0x112 },
        { "scf",         required_argument, 0, 0x113 },

        { "disco",       no_argument,       0, 0x114 },
        { 0, 0, 0, 0 }
    };

    /* TODO: getopt does not support -sf, -sb etc..
     * Either break the interface and make them --sf, --sb (like they are now),
     * or parse them before running getopt.. */

    for (;;) {
        int32_t opt = getopt_long(*argc, *argv, "hviwl:I:p:P:I:bfm:n", opts, NULL);
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
                client->filter_mode = BM_FILTER_MODE_DMENU_CASE_INSENSITIVE;
                break;
            case 'w':
                client->wrap = true;
                break;
            case 'l':
                client->lines = strtol(optarg, NULL, 10);
                break;
            case 'p':
                client->title = optarg;
                break;
            case 'P':
                client->prefix = optarg;
                break;
            case 'I':
                client->selected = strtol(optarg, NULL, 10);
                break;
            case 0x100:
                client->scrollbar = (!strcmp(optarg, "always") ? BM_SCROLLBAR_ALWAYS : (!strcmp(optarg, "autohide") ? BM_SCROLLBAR_AUTOHIDE : BM_SCROLLBAR_NONE));
                break;
            case 0x115:
                client->ifne = true;
                break;

            case 'b':
                client->bottom = true;
                break;
            case 'f':
                client->grab = true;
                break;
            case 'm':
                client->monitor = strtol(optarg, NULL, 10);
                break;
            case 'n':
                client->no_overlap = true;
                break;

            case 0x101:
                client->font = optarg;
                break;
            case 0x102:
                client->colors[BM_COLOR_TITLE_BG] = optarg;
                break;
            case 0x103:
                client->colors[BM_COLOR_TITLE_FG] = optarg;
                break;
            case 0x104:
                client->colors[BM_COLOR_FILTER_BG] = optarg;
                break;
            case 0x105:
                client->colors[BM_COLOR_FILTER_FG] = optarg;
                break;
            case 0x106:
                client->colors[BM_COLOR_ITEM_BG] = optarg;
                break;
            case 0x107:
                client->colors[BM_COLOR_ITEM_FG] = optarg;
                break;
            case 0x108:
                client->colors[BM_COLOR_HIGHLIGHTED_BG] = optarg;
                break;
            case 0x109:
                client->colors[BM_COLOR_HIGHLIGHTED_FG] = optarg;
                break;
            case 0x110:
                client->colors[BM_COLOR_SELECTED_BG] = optarg;
                break;
            case 0x111:
                client->colors[BM_COLOR_SELECTED_FG] = optarg;
                break;
            case 0x112:
                client->colors[BM_COLOR_SCROLLBAR_BG] = optarg;
                break;
            case 0x113:
                client->colors[BM_COLOR_SCROLLBAR_FG] = optarg;
                break;

            case 0x114:
                disco();
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

struct bm_menu*
menu_with_options(const struct client *client)
{
    struct bm_menu *menu;
    if (!(menu = bm_menu_new(NULL)))
        return NULL;

    bm_menu_set_font(menu, client->font);
    bm_menu_set_title(menu, client->title);
    bm_menu_set_prefix(menu, client->prefix);
    bm_menu_set_filter_mode(menu, client->filter_mode);
    bm_menu_set_lines(menu, client->lines);
    bm_menu_set_wrap(menu, client->wrap);
    bm_menu_set_bottom(menu, client->bottom);
    bm_menu_set_monitor(menu, client->monitor);
    bm_menu_set_scrollbar(menu, client->scrollbar);

    for (uint32_t i = 0; i < BM_COLOR_LAST; ++i)
        bm_menu_set_color(menu, i, client->colors[i]);

    if (client->grab) {
        bm_menu_set_filter(menu, "Loading...");
        // bm_menu_grab_keyboard(menu, true);
        bm_menu_render(menu);
        bm_menu_set_filter(menu, NULL);
    }

    return menu;
}

enum bm_run_result
run_menu(const struct client *client, struct bm_menu *menu, void (*item_cb)(struct bm_item *item, const char *text))
{
    bm_menu_set_highlighted_index(menu, client->selected);
    bm_menu_grab_keyboard(menu, true);
    bm_menu_set_panel_overlap(menu, !client->no_overlap);

    if (client->ifne && !bm_menu_get_items(menu, NULL))
        return BM_RUN_RESULT_CANCEL;

    uint32_t unicode;
    enum bm_key key;
    enum bm_run_result status = BM_RUN_RESULT_RUNNING;
    do {
        bm_menu_render(menu);
        key = bm_menu_poll_key(menu, &unicode);
    } while ((status = bm_menu_run_with_key(menu, key, unicode)) == BM_RUN_RESULT_RUNNING);

    if (status == BM_RUN_RESULT_SELECTED) {
        uint32_t i, count;
        struct bm_item **items = bm_menu_get_selected_items(menu, &count);
        for (i = 0; i < count; ++i) {
            const char *text = bm_item_get_text(items[i]);
            item_cb(items[i], text);
        }

        if (!count && bm_menu_get_filter(menu))
            item_cb(NULL, bm_menu_get_filter(menu));
    }

    return status;
}

/* vim: set ts=8 sw=4 tw=0 :*/
