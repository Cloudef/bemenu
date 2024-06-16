#include "internal.h"
#include <wchar.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <dlfcn.h>
#include <assert.h>
#include <math.h>

#define _XOPEN_SOURCE_EXTENDED
#define NCURSES_WIDECHAR 1
#include <curses.h>

#if _WIN32
static const char *TTY = "CON";
#else
static const char *TTY = "/dev/tty";
#endif

#if NCURSES_EXT_FUNCS < 20150808
#   define set_escdelay(x) ESCDELAY = (x)
#endif

static struct curses {
    WINDOW *stdscreen;
    struct sigaction abrt_action;
    struct sigaction segv_action;
    struct sigaction winch_action;
    char *buffer;
    size_t blen;
    int old_stdin;
    int old_stdout;
    bool polled_once;
    bool should_terminate;
} curses;

static inline void ignore_ret(int useless, ...) { (void)useless; }

static void
reopen_stdin(void)
{
    ignore_ret(0, freopen(TTY, "r", stdin));
}

static void
reopen_stdin_stdout(void)
{
    reopen_stdin();
    ignore_ret(0, freopen(TTY, "w", stdout));
}

static void
store_stdin_stdout(void)
{
    curses.old_stdin = dup(STDIN_FILENO);
    curses.old_stdout = dup(STDOUT_FILENO);
}

static void
restore_stdin(void)
{
    if (curses.old_stdin != -1) {
        dup2(curses.old_stdin, STDIN_FILENO);
        close(curses.old_stdin);
        curses.old_stdin = -1;
    }
}

static void
restore_stdin_stdout(void)
{
    restore_stdin();

    if (curses.old_stdout != -1) {
        dup2(curses.old_stdout, STDOUT_FILENO);
        close(curses.old_stdout);
        curses.old_stdout = -1;
    }
}

static void
terminate(void)
{
    if (curses.buffer) {
        free(curses.buffer);
        curses.buffer = NULL;
        curses.blen = 0;
    }

    if (!curses.stdscreen)
        return;

    reopen_stdin_stdout();
    refresh();
    endwin();
    restore_stdin_stdout();
    curses.stdscreen = NULL;
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
    if (!curses.stdscreen)
        return;

    refresh();
    endwin();
}

BM_LOG_ATTR(3, 4) static void
draw_line(int32_t pair, int32_t y, const char *fmt, ...)
{
    assert(fmt);

    size_t ncols;
    if ((ncols = getmaxx(curses.stdscreen)) <= 0)
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

static bool
render(struct bm_menu *menu)
{
    if (curses.should_terminate) {
        terminate();
        curses.should_terminate = false;
    }

    if (!curses.stdscreen) {
        store_stdin_stdout();
        reopen_stdin_stdout();
        setlocale(LC_CTYPE, "");

        if ((curses.stdscreen = initscr()) == NULL)
            return true;

        set_escdelay(25);
        flushinp();
        keypad(curses.stdscreen, true);
        curs_set(1);
        noecho();
        raw();

        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLACK, COLOR_RED);
        init_pair(2, COLOR_RED, -1);
    }

    erase();

    uint32_t ncols = getmaxx(curses.stdscreen);
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

    const char *filter_text = (menu->filter ? menu->filter + doffset : "");
    if (menu->password) {
        draw_line(0, 0, "%*s", title_len, "");
    } else {
        draw_line(0, 0, "%*s%s", title_len, "", filter_text);
    }

    if (menu->title && title_len > 0) {
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "%s", menu->title);
        attroff(COLOR_PAIR(1));
    }

    uint32_t count, cl = 0;
    const uint32_t lines = fmax(getmaxy(curses.stdscreen), 1) - 1;
    if (lines > 1) {
        struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
        const bool scrollbar = (menu->scrollbar > BM_SCROLLBAR_NONE && (menu->scrollbar != BM_SCROLLBAR_AUTOHIDE || count > lines) ? true : false);
        const int32_t offset_x = title_len + (scrollbar && 2 > title_len ? 2 - title_len : 0);
        const int32_t prefix_x = (menu->prefix ? bm_utf8_string_screen_width(menu->prefix) : 0);

        const uint32_t page = menu->index / lines * lines;
        for (uint32_t i = page; i < count && cl < lines; ++i) {
            bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));
            int32_t color = (highlighted ? 2 : (bm_menu_item_is_selected(menu, items[i]) ? 1 : 0));

            if (menu->prefix && highlighted) {
                draw_line(color, 1 + cl++, "%*s%s %s", offset_x, "", menu->prefix, (items[i]->text ? items[i]->text : ""));
            } else {
                draw_line(color, 1 + cl++, "%*s%s%s", offset_x + prefix_x, "", (menu->prefix ? " " : ""), (items[i]->text ? items[i]->text : ""));
            }

        }

        if (scrollbar) {
            attron(COLOR_PAIR(1));
            const float percent = fmin(((float)page / (count - lines)), 1.0f);
            const uint32_t size = fmax(lines * ((float)lines / count), 1.0f);
            const uint32_t posy = percent * (lines - size);
            for (uint32_t i = 0; i < size; ++i)
                mvprintw(1 + posy + i, 0, "▒");
            attroff(COLOR_PAIR(1));
        }
    }

    move(0, title_len + (menu->curses_cursor < ccols ? menu->curses_cursor : ccols));
    refresh();

    // Make it possible to read stdin even after rendering
    // Only make it impossible to read original stdin after poll_key is called once
    // This is mainly to make -f work even on curses backend
    if (!curses.polled_once) {
        reopen_stdin();
        restore_stdin();
        curses.should_terminate = true;
    }

    return true;
}

static uint32_t
get_displayed_count(const struct bm_menu *menu)
{
    (void)menu;
    return (curses.stdscreen ? getmaxy(curses.stdscreen) : 0);
}

static enum bm_key
poll_key(const struct bm_menu *menu, uint32_t *unicode)
{
    (void)menu;
    assert(unicode);
    *unicode = 0;
    curses.polled_once = true;

    if (!curses.stdscreen || curses.should_terminate)
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

        case 22: /* C-v */
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
        case 13: /* C-m */
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
    assert(!curses.stdscreen && "bemenu supports only one curses instance");

    memset(&curses, 0, sizeof(curses));
    curses.old_stdin = -1;
    curses.old_stdout = -1;

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = crash_handler;
    sigaction(SIGABRT, &action, &curses.abrt_action);
    sigaction(SIGSEGV, &action, &curses.segv_action);

    action.sa_handler = resize_handler;
    sigaction(SIGWINCH, &action, &curses.winch_action);
    return true;
}

BM_PUBLIC extern const char*
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
