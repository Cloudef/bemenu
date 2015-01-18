#include "internal.h"
#include "version.h"

#include <wchar.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <dlfcn.h>
#include <assert.h>

#include <ncurses.h>

#if _WIN32
static const char *TTY = "CON";
#else
static const char *TTY = "/dev/tty";
#endif

static struct curses {
    WINDOW *stdscr;
    struct sigaction abrt_action;
    struct sigaction segv_action;
    struct sigaction winch_action;
    char *buffer;
    size_t blen;
    int old_stdin;
    int old_stdout;
} curses;

static void
terminate(void)
{
    if (curses.buffer) {
        free(curses.buffer);
        curses.buffer = NULL;
        curses.blen = 0;
    }

    if (!curses.stdscr)
        return;

    freopen(TTY, "w", stdout);

    refresh();
    endwin();

    dup2(curses.old_stdin, STDIN_FILENO);
    dup2(curses.old_stdout, STDOUT_FILENO);
    close(curses.old_stdin);
    close(curses.old_stdout);

    curses.stdscr = NULL;
}

static void
crash_handler(int sig)
{
    (void)sig;
    terminate();
}

static void
resize_handler(int sig)
{
    (void)sig;
    if (!curses.stdscr)
        return;

    endwin();
    refresh();
}

BM_LOG_ATTR(3, 4) static void
draw_line(int32_t pair, int32_t y, const char *fmt, ...)
{
    assert(fmt);

    size_t ncols;
    if ((ncols = getmaxx(curses.stdscr)) <= 0)
        return;

    va_list args;
    va_start(args, fmt);
    bool ret = bm_vrprintf(&curses.buffer, &curses.blen, fmt, args);
    va_end(args);

    if (!ret)
        return;

    size_t nlen = strlen(curses.buffer);
    size_t dw = 0, i = 0;
    while (dw < ncols && i < nlen) {
        if (curses.buffer[i] == '\t') curses.buffer[i] = ' ';
        int32_t next = bm_utf8_rune_next(curses.buffer, i);
        dw += bm_utf8_rune_width(curses.buffer + i, next);
        i += (next ? next : 1);
    }

    if (dw < ncols) {
        /* line is too short, widen it */
        size_t offset = i + (ncols - dw);
        if (curses.blen <= offset && !bm_resize_buffer(&curses.buffer, &curses.blen, offset + 1))
             return;

        memset(curses.buffer + nlen, ' ', offset - nlen);
        curses.buffer[offset] = 0;
    } else if (i < curses.blen) {
        /* line is too long, shorten it */
        i -= bm_utf8_rune_prev(curses.buffer, i - (dw - ncols)) - 1;
        size_t cc = dw - (dw - ncols);

        size_t offset = i - (dw - ncols) + (ncols - cc) + 1;
        if (curses.blen <= offset) {
            int32_t diff = offset - curses.blen + 1;
            if (!bm_resize_buffer(&curses.buffer, &curses.blen, curses.blen + diff))
                return;
        }

        memset(curses.buffer + i - (dw - ncols), ' ', (ncols - cc) + 1);
        curses.buffer[offset] = 0;
    }

    if (pair > 0)
        attron(COLOR_PAIR(pair));

    mvprintw(y, 0, "%s", curses.buffer);

    if (pair > 0)
        attroff(COLOR_PAIR(pair));
}

static void
render(const struct bm_menu *menu)
{
    if (!curses.stdscr) {
        curses.old_stdin = dup(STDIN_FILENO);
        curses.old_stdout = dup(STDOUT_FILENO);

        freopen(TTY, "w", stdout);
        freopen(TTY, "r", stdin);

        setlocale(LC_CTYPE, "");

        if ((curses.stdscr = initscr()) == NULL)
            return;

        ESCDELAY = 25;
        flushinp();
        keypad(curses.stdscr, true);
        curs_set(1);
        noecho();
        raw();

        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLACK, COLOR_RED);
        init_pair(2, COLOR_RED, -1);
    }

    erase();

    uint32_t ncols = getmaxx(curses.stdscr);
    uint32_t title_len = (menu->title ? strlen(menu->title) + 1 : 0);

    if (title_len >= ncols)
        title_len = 0;

    uint32_t ccols = ncols - title_len - 1;
    uint32_t dcols = 0, doffset = menu->cursor;

    while (doffset > 0 && dcols < ccols) {
        int prev = bm_utf8_rune_prev(menu->filter, doffset);
        dcols += bm_utf8_rune_width(menu->filter + doffset - prev, prev);
        doffset -= (prev ? prev : 1);
    }

    draw_line(0, 0, "%*s%s", title_len, "", (menu->filter ? menu->filter + doffset : ""));

    if (menu->title && title_len > 0) {
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, menu->title);
        attroff(COLOR_PAIR(1));
    }

    uint32_t count, cl = 1;
    const uint32_t lines = getmaxy(curses.stdscr);
    if (lines > 1) {
        uint32_t displayed = 0;
        struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
        const bool scrollbar = (menu->scrollbar > BM_SCROLLBAR_NONE && (menu->scrollbar != BM_SCROLLBAR_AUTOHIDE || count > lines) ? true : false);
        const int32_t offset_x = (scrollbar ? 2 : 0);
        const int32_t prefix_x = (menu->prefix ? bm_utf8_string_screen_width(menu->prefix) : 0);
        for (uint32_t i = (menu->index / (lines - 1)) * (lines - 1); i < count && cl < lines; ++i) {
            bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));
            int32_t color = (highlighted ? 2 : (bm_menu_item_is_selected(menu, items[i]) ? 1 : 0));

            if (menu->prefix && highlighted) {
                draw_line(color, cl++, "%*s%s %s", offset_x, "", menu->prefix, (items[i]->text ? items[i]->text : ""));
            } else {
                draw_line(color, cl++, "%*s%s%s", offset_x + prefix_x, "", (menu->prefix ? " " : ""), (items[i]->text ? items[i]->text : ""));
            }

            ++displayed;
        }

        if (scrollbar) {
            attron(COLOR_PAIR(1));
            uint32_t percent = (menu->index / (float)(count - 1)) * (displayed - 1);
            mvprintw(1 + percent, 0, "▒");
            attroff(COLOR_PAIR(1));
        }
    }

    move(0, title_len + (menu->curses_cursor < ccols ? menu->curses_cursor : ccols));
    refresh();
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    (void)menu;
    return (curses.stdscr ? getmaxy(curses.stdscr) : 0);
}

static enum bm_key
poll_key(const struct bm_menu *menu, uint32_t *unicode)
{
    (void)menu;
    assert(unicode);
    *unicode = 0;

    if (!curses.stdscr)
        return BM_KEY_NONE;

    get_wch((wint_t*)unicode);

    switch (*unicode) {
#if KEY_RESIZE
        case KEY_RESIZE:
            return BM_KEY_NONE;
#endif

        case 16: /* C-p */
        case KEY_UP:
            return BM_KEY_UP;

        case 14: /* C-n */
        case KEY_DOWN:
            return BM_KEY_DOWN;

        case 2: /* C-b */
        case KEY_LEFT:
            return BM_KEY_LEFT;

        case 6: /* C-f */
        case KEY_RIGHT:
            return BM_KEY_RIGHT;

        case 1: /* C-a */
        case 391: /* S-Home */
        case KEY_HOME:
            return BM_KEY_HOME;

        case 5: /* C-e */
        case 386: /* S-End */
        case KEY_END:
            return BM_KEY_END;

        case KEY_PPAGE: /* Page up */
            return BM_KEY_PAGE_UP;

        case KEY_NPAGE: /* Page down */
            return BM_KEY_PAGE_DOWN;

        case 550: /* C-Page up */
        case 398: /* S-Page up */
            return BM_KEY_SHIFT_PAGE_UP;

        case 545: /* C-Page down */
        case 396: /* S-Page down */
            return BM_KEY_SHIFT_PAGE_DOWN;

        case 8: /* C-h */
        case 127: /* Delete */
        case KEY_BACKSPACE:
            return BM_KEY_BACKSPACE;

        case 4: /* C-d */
        case KEY_DC:
            return BM_KEY_DELETE;

        case 383: /* S-Del */
        case 21: /* C-u */
            return BM_KEY_LINE_DELETE_LEFT;

        case 11: /* C-k */
            return BM_KEY_LINE_DELETE_RIGHT;

        case 23: /* C-w */
            return BM_KEY_WORD_DELETE;

        case 9: /* Tab */
            return BM_KEY_TAB;

        case 353: /* S-Tab */
            return BM_KEY_SHIFT_TAB;

        case 18: /* C-r */
            return BM_KEY_CONTROL_RETURN;

        case 20: /* C-t */
        case 331: /* Insert */
            terminate();
            return BM_KEY_SHIFT_RETURN;

        case 10: /* Return */
            terminate();
            return BM_KEY_RETURN;

        case 7: /* C-g */
        case 27: /* Escape */
            terminate();
            return BM_KEY_ESCAPE;

        default: break;
    }

    return BM_KEY_UNICODE;
}

static void
destructor(struct bm_menu *menu)
{
    (void)menu;
    terminate();
    sigaction(SIGABRT, &curses.abrt_action, NULL);
    sigaction(SIGSEGV, &curses.segv_action, NULL);
    sigaction(SIGWINCH, &curses.winch_action, NULL);
    memset(&curses, 0, sizeof(curses));
}

static bool
constructor(struct bm_menu *menu)
{
    (void)menu;
    assert(!curses.stdscr && "bemenu supports only one curses instance");

    memset(&curses, 0, sizeof(curses));

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = crash_handler;
    sigaction(SIGABRT, &action, &curses.abrt_action);
    sigaction(SIGSEGV, &action, &curses.segv_action);

    action.sa_handler = resize_handler;
    sigaction(SIGWINCH, &action, &curses.winch_action);
    return true;
}

extern const char*
register_renderer(struct render_api *api)
{
    api->constructor = constructor;
    api->destructor = destructor;
    api->get_displayed_count = get_displayed_count;
    api->poll_key = poll_key;
    api->render = render;
    api->priorty = BM_PRIO_TERMINAL;
    api->version = BM_PLUGIN_VERSION;
    return "curses";
}

/* vim: set ts=8 sw=4 tw=0 :*/
