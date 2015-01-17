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
          " --backend             options: curses, wayland, x11\n"
          " --prioritory          options: terminal, gui\n\n"

          "Backend specific options\n"
          "   c = ncurses, w == wayland, x == x11\n"
          "   (...) At end of help indicates the backend support for option.\n\n"

          " -b, --bottom          appears at the bottom of the screen. (x)\n"
          " -f, --grab            grabs the keyboard before reading stdin. (x)\n"
          " -m, --monitor         index of monitor where menu will appear. (x)\n"
          " --fn                  defines the font to be used ('name [size]'). (wx)\n"
          " --bg                  defines the background color. (wx)\n"
          " --tb                  defines the title background color. (wx)\n"
          " --tf                  defines the title foreground color. (wx)\n"
          " --fb                  defines the filter background color. (wx)\n"
          " --ff                  defines the filter foreground color. (wx)\n"
          " --nb                  defines the normal background color. (wx)\n"
          " --nf                  defines the normal foreground color. (wx)\n"
          " --hb                  defines the highlighted background color. (wx)\n"
          " --hf                  defines the highlighted foreground color. (wx)\n"
          " --sb                  defines the selected background color. (wx)\n"
          " --sf                  defines the selected foreground color. (wx)\n", out);

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
        { "backend",     required_argument, 0, 0x100 },
        { "prioritory",  required_argument, 0, 0x101 },

        { "bottom",      no_argument,       0, 'b' },
        { "grab",        no_argument,       0, 'f' },
        { "monitor",     required_argument, 0, 'm' },
        { "fn",          required_argument, 0, 0x102 },
        { "bg",          required_argument, 0, 0x103 },
        { "tb",          required_argument, 0, 0x104 },
        { "tf",          required_argument, 0, 0x105 },
        { "fb",          required_argument, 0, 0x106 },
        { "ff",          required_argument, 0, 0x107 },
        { "nb",          required_argument, 0, 0x108 },
        { "nf",          required_argument, 0, 0x109 },
        { "hb",          required_argument, 0, 0x110 },
        { "hf",          required_argument, 0, 0x111 },
        { "sb",          required_argument, 0, 0x112 },
        { "sf",          required_argument, 0, 0x113 },

        { "disco",       no_argument,       0, 0x114 },
        { 0, 0, 0, 0 }
    };

    /* TODO: getopt does not support -sf, -sb etc..
     * Either break the interface and make them --sf, --sb (like they are now),
     * or parse them before running getopt.. */

    for (;;) {
        int32_t opt = getopt_long(*argc, *argv, "hviwl:I:p:P:I:bfm:", opts, NULL);
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
                client->wrap = 1;
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
                client->renderer = optarg;
                break;

            case 0x101:
                if (!strcmp(optarg, "terminal"))
                    client->prioritory = BM_PRIO_TERMINAL;
                else if (!strcmp(optarg, "gui"))
                    client->prioritory = BM_PRIO_GUI;
                break;

            case 'b':
                client->bottom = 1;
                break;
            case 'f':
                client->grab = 1;
                break;
            case 'm':
                client->monitor = strtol(optarg, NULL, 10);
                break;

            case 0x102:
                client->font = optarg;
                break;
            case 0x103:
                client->colors[BM_COLOR_BG] = optarg;
                break;
            case 0x104:
                client->colors[BM_COLOR_TITLE_BG] = optarg;
                break;
            case 0x105:
                client->colors[BM_COLOR_TITLE_FG] = optarg;
                break;
            case 0x106:
                client->colors[BM_COLOR_FILTER_BG] = optarg;
                break;
            case 0x107:
                client->colors[BM_COLOR_FILTER_FG] = optarg;
                break;
            case 0x108:
                client->colors[BM_COLOR_ITEM_BG] = optarg;
                break;
            case 0x109:
                client->colors[BM_COLOR_ITEM_FG] = optarg;
                break;
            case 0x110:
                client->colors[BM_COLOR_HIGHLIGHTED_BG] = optarg;
                break;
            case 0x111:
                client->colors[BM_COLOR_HIGHLIGHTED_FG] = optarg;
                break;
            case 0x112:
                client->colors[BM_COLOR_SELECTED_BG] = optarg;
                break;
            case 0x113:
                client->colors[BM_COLOR_SELECTED_FG] = optarg;
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
menu_with_options(struct client *client)
{
    struct bm_menu *menu;
    if (!(menu = bm_menu_new(client->renderer, client->prioritory)))
        return NULL;

    bm_menu_set_font(menu, client->font);
    bm_menu_set_title(menu, client->title);
    bm_menu_set_prefix(menu, client->prefix);
    bm_menu_set_filter_mode(menu, client->filter_mode);
    bm_menu_set_lines(menu, client->lines);
    bm_menu_set_wrap(menu, client->wrap);
    bm_menu_set_bottom(menu, client->bottom);
    bm_menu_set_monitor(menu, client->monitor);

    for (uint32_t i = 0; i < BM_COLOR_LAST; ++i)
        bm_menu_set_color(menu, i, client->colors[i]);

    if (client->grab)
        bm_menu_grab_keyboard(menu, true);

    return menu;
}

enum bm_run_result
run_menu(struct bm_menu *menu)
{
    bm_menu_grab_keyboard(menu, true);

    uint32_t unicode;
    enum bm_key key;
    enum bm_run_result status = BM_RUN_RESULT_RUNNING;
    do {
        bm_menu_render(menu);
        key = bm_menu_poll_key(menu, &unicode);
    } while ((status = bm_menu_run_with_key(menu, key, unicode)) == BM_RUN_RESULT_RUNNING);
    return status;
}

/* vim: set ts=8 sw=4 tw=0 :*/
