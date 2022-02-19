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
          " -v, --version         display version.\n", out);

    fputs("\n"
          "Use BEMENU_BACKEND env variable to force backend:\n"
          " curses               ncurses based terminal backend\n"
          " wayland              wayland backend\n"
          " x11                  x11 backend\n\n"

          "If BEMENU_BACKEND is empty, one of the GUI backends is selected automatically.\n\n"

          "Backend specific options\n"
          "   c = ncurses, w == wayland, x == x11\n"
          "   (...) At end of help indicates the backend support for option.\n\n", out);

    exit((out == stderr ? EXIT_FAILURE : EXIT_SUCCESS));
}

struct bm_menu*
menu_with_options(struct client *client)
{
    struct bm_menu *menu;
    if (!(menu = bm_menu_new(NULL)))
        return NULL;

    client->fork = (client->force_fork || (bm_renderer_get_priorty(bm_menu_get_renderer(menu)) != BM_PRIO_TERMINAL));

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
    struct bm_pointer pointer;
    struct bm_touch touch;
    enum bm_run_result status = BM_RUN_RESULT_RUNNING;
    do {
        bm_menu_render(menu);
        key = bm_menu_poll_key(menu, &unicode);
        pointer = bm_menu_poll_pointer(menu);
        touch = bm_menu_poll_touch(menu);
    } while ((status = bm_menu_run_with_events(menu, key, pointer, touch, unicode)) == BM_RUN_RESULT_RUNNING);

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
