#include "internal.h"

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
    fprintf(stderr, "\x1B[?25h");
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
    fprintf(stderr, "\x1B[?25l");
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
    fprintf(stderr, "\x1B[?25h");
    exit(EXIT_SUCCESS);
}

#define WHITESPACE " \t\n\r"

static const char*
tokenize(const char *cstr, size_t *out_len, const char *separator, bool skip_whitespace, const char **state)
{
   assert(out_len && separator && state);
   const char *current = (state && *state ? *state : cstr);

   if (!current || !*current || !cstr || !*cstr)
      return NULL;

   current += strspn(current, separator);

   if (skip_whitespace)
      current += strspn(current, WHITESPACE);

   *out_len = strcspn(current, separator);
   *state = current + *out_len;

   if (skip_whitespace) {
      const size_t ws = strcspn(current, WHITESPACE);
      *out_len -= (ws < *out_len ? *out_len - ws : 0);
   }

   return current;
}

static const char*
tokenize_quoted(const char *cstr, size_t *out_len, const char *separator, const char *quotes, const char **state)
{
    assert(out_len && separator && quotes && state);
    const char *e, *current = tokenize(cstr, out_len, separator, true, state);

    if (!current)
        return NULL;

    for (const char *q = quotes; *q; ++q) {
        if (*current != *q)
            continue;

        bool escaped = false;
        for (e = ++current; *e; ++e) {
            if (escaped)
                escaped = false;
            else if (*e == '\\')
                escaped = true;
            else if (*e == *q)
                break;
        }

        *out_len = e - current;
        e = (!*e ? e : e + 1);

        if (*e) {
            size_t tmp;
            const char *state2 = NULL;
            *state = tokenize(e, &tmp, separator, true, &state2);
        } else {
            *state = e;
        }

        break;
    }

    return current;
}

char*
cstrcopy(const char *str, size_t size)
{
    char *cpy = calloc(1, size + 1);
    return (cpy ? memcpy(cpy, str, size) : NULL);
}

char**
tokenize_quoted_to_argv(const char *str, char *argv0, int *out_argc)
{
    if (out_argc) *out_argc = 0;

    size_t count = !!argv0;
    {
        size_t len;
        const char *state = NULL;
        while (tokenize_quoted(str, &len, " ", "\"'", &state))
            ++count;
    }

    char **tokens;
    if (!count || !(tokens = calloc(count + 1, sizeof(char*))))
        return NULL;

    {
        tokens[0] = argv0;
        size_t i = !!argv0, len;
        const char *t, *state = NULL;
        while (i < count && (t = tokenize_quoted(str, &len, " ", "\"'", &state))) {
            if (!(tokens[i++] = cstrcopy(t, len)))
                return NULL;
        }
    }

    if (out_argc) *out_argc = count;
    return tokens;
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
          " -F, --filter          filter entries for a given string.\n"
          " -w, --wrap            wraps cursor selection.\n"
          " -l, --list            list items vertically with the given number of lines.\n"
          " -p, --prompt          defines the prompt text to be displayed.\n"
          " -P, --prefix          text to show before highlighted item.\n"
          " -I, --index           select item at index automatically.\n"
          " -x, --password        hide input.\n"
          " -s, --no-spacing      disable the title spacing on entries.\n"
          " --scrollbar           display scrollbar. (none (default), always, autohide)\n"
          " --ifne                only display menu if there are items.\n"
          " --fork                always fork. (bemenu-run)\n"
          " --no-exec             do not execute command. (bemenu-run)\n\n"

          "Use BEMENU_BACKEND env variable to force backend:\n"
          " curses               ncurses based terminal backend\n"
          " wayland              wayland backend\n"
          " x11                  x11 backend\n\n"

          "If BEMENU_BACKEND is empty, one of the GUI backends is selected automatically.\n\n"

          "Backend specific options\n"
          "   c = ncurses, w == wayland, x == x11\n"
          "   (...) At end of help indicates the backend support for option.\n\n"

          " -b, --bottom          appears at the bottom of the screen. (wx)\n"
          " -c, --center          appears at the center of the screen. (wx)\n"
          " -f, --grab            show the menu before reading stdin. (wx)\n"
          " -n, --no-overlap      adjust geometry to not overlap with panels. (w)\n"
          " -m, --monitor         index of monitor where menu will appear. (wx)\n"
          " -H, --line-height     defines the height to make each menu line (0 = default height). (wx)\n"
          " -M, --margin          defines the empty space on either side of the menu. (wx)"
          " --ch                  defines the height of the cursor (0 = scales with line height). (wx)\n"
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

static void
set_monitor(struct client *client, char *arg)
{
    char *endptr = NULL;
    long num = strtol(arg, &endptr, 10);
    if (arg == endptr) { // No digits found
        if (!strcmp(arg, "focused")) {
            client->monitor = -1;
        } else if (!strcmp(arg, "all")) {
            client->monitor = -2;
        } else {
            client->monitor_name = arg;
        }
    } else {
        client->monitor = num;
    }
}

static void
do_getopt(struct client *client, int *argc, char **argv[])
{
    assert(client && argc && argv);

    static const struct option opts[] = {
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },

        { "ignorecase",  no_argument,       0, 'i' },
        { "filter",      required_argument, 0, 'F' },
        { "wrap",        no_argument,       0, 'w' },
        { "list",        required_argument, 0, 'l' },
        { "center",      no_argument,       0, 'c' },
        { "prompt",      required_argument, 0, 'p' },
        { "index",       required_argument, 0, 'I' },
        { "prefix",      required_argument, 0, 'P' },
        { "password",    no_argument,       0, 'x' },
        { "scrollbar",   required_argument, 0, 0x100 },
        { "ifne",        no_argument,       0, 0x115 },
        { "fork",        no_argument,       0, 0x116 },
        { "no-exec",     no_argument,       0, 0x117 },

        { "bottom",      no_argument,       0, 'b' },
        { "grab",        no_argument,       0, 'f' },
        { "no-overlap",  no_argument,       0, 'n' },
        { "no-spacing",  no_argument,       0, 's' },
        { "monitor",     required_argument, 0, 'm' },
        { "line-height", required_argument, 0, 'H' },
        { "margin",      required_argument, 0, 'M' },
        { "ch",          required_argument, 0, 0x118 },
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

    /* Set default monitor index to -1 (maybe be overwritten with -m below). */
    client->monitor = -1;

    for (optind = 0;;) {
        int32_t opt;

        if ((opt = getopt_long(*argc, *argv, "hviwxcl:I:p:P:I:bfm:H:M:ns", opts, NULL)) < 0)
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
            case 'F':
                client->initial_filter = optarg;
                break;
            case 'w':
                client->wrap = true;
                break;
            case 'l':
                client->lines = strtol(optarg, NULL, 10);
                break;
            case 'c':
                client->center = true;
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
                client->scrollbar = (!strcmp(optarg, "none") ? BM_SCROLLBAR_NONE : (!strcmp(optarg, "always") ? BM_SCROLLBAR_ALWAYS : (!strcmp(optarg, "autohide") ? BM_SCROLLBAR_AUTOHIDE : BM_SCROLLBAR_NONE)));
                break;
            case 0x115:
                client->ifne = true;
                break;
            case 0x116:
                client->force_fork = true;
                break;
            case 0x117:
                client->no_exec = true;
                break;
            case 'x':
                client->password = true;
                break;

            case 'b':
                client->bottom = true;
                break;
            case 'f':
                client->grab = true;
                break;
            case 'm':
                set_monitor(client, optarg);
                break;
            case 'n':
                client->no_overlap = true;
                break;
            case 's':
                client->no_spacing = true;
                break;

            case 'H':
                client->line_height = strtol(optarg, NULL, 10);
                break;
            case 'M':
                client->hmargin_size = strtol(optarg, NULL, 10);
                break;
            case 0x118:
                client->cursor_height = strtol(optarg, NULL, 10);
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

void
parse_args(struct client *client, int *argc, char **argv[])
{
    int num_opts;
    char **opts;
    const char *env;
    if ((env = getenv("BEMENU_OPTS")) && (opts = tokenize_quoted_to_argv(env, (*argv)[0], &num_opts)))
        do_getopt(client, &num_opts, &opts);
    do_getopt(client, argc, argv);
}

struct bm_menu*
menu_with_options(struct client *client)
{
    struct bm_menu *menu;
    if (!(menu = bm_menu_new(NULL)))
        return NULL;

    client->fork = (client->force_fork || (bm_renderer_get_priorty(bm_menu_get_renderer(menu)) != BM_PRIO_TERMINAL)); 

    bm_menu_set_font(menu, client->font);
    bm_menu_set_line_height(menu, client->line_height);
    bm_menu_set_cursor_height(menu, client->cursor_height);
    bm_menu_set_title(menu, client->title);
    bm_menu_set_prefix(menu, client->prefix);
    bm_menu_set_filter_mode(menu, client->filter_mode);
    bm_menu_set_lines(menu, client->lines);
    bm_menu_set_wrap(menu, client->wrap);
    bm_menu_set_monitor(menu, client->monitor);
    bm_menu_set_monitor_name(menu, client->monitor_name);
    bm_menu_set_scrollbar(menu, client->scrollbar);
    bm_menu_set_panel_overlap(menu, !client->no_overlap);
    bm_menu_set_spacing(menu, !client->no_spacing);
    bm_menu_set_password(menu, client->password);
    bm_menu_set_hmargin_size(menu, client->hmargin_size);

    if (client->center) {
        bm_menu_set_align(menu, BM_ALIGN_CENTER);
    } else if (client->bottom) {
        bm_menu_set_align(menu, BM_ALIGN_BOTTOM);
    } else {
        bm_menu_set_align(menu, BM_ALIGN_TOP);
    }

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
run_menu(const struct client *client, struct bm_menu *menu, void (*item_cb)(const struct client *client, struct bm_item *item))
{
    bm_menu_set_highlighted_index(menu, client->selected);
    bm_menu_grab_keyboard(menu, true);
    bm_menu_set_filter(menu, client->initial_filter);

    if (client->ifne && !bm_menu_get_items(menu, NULL))
        return BM_RUN_RESULT_CANCEL;

    uint32_t unicode;
    enum bm_key key;
    enum bm_run_result status = BM_RUN_RESULT_RUNNING;
    do {
        bm_menu_render(menu);
        key = bm_menu_poll_key(menu, &unicode);
    } while ((status = bm_menu_run_with_key(menu, key, unicode)) == BM_RUN_RESULT_RUNNING);

    switch (status) {
        case BM_RUN_RESULT_SELECTED:
        case BM_RUN_RESULT_CUSTOM_1:
        case BM_RUN_RESULT_CUSTOM_2:
        case BM_RUN_RESULT_CUSTOM_3:
        case BM_RUN_RESULT_CUSTOM_4:
        case BM_RUN_RESULT_CUSTOM_5:
        case BM_RUN_RESULT_CUSTOM_6:
        case BM_RUN_RESULT_CUSTOM_7:
        case BM_RUN_RESULT_CUSTOM_8:
        case BM_RUN_RESULT_CUSTOM_9:
        case BM_RUN_RESULT_CUSTOM_10:
            {
            uint32_t i, count;
            struct bm_item **items = bm_menu_get_selected_items(menu, &count);
            for (i = 0; i < count; ++i) item_cb(client, items[i]);
            }
            break;
        default: break;
    }

    return status;
}

/* vim: set ts=8 sw=4 tw=0 :*/
