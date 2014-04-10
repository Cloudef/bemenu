/**
 * @file curses.c
 */

#include "../internal.h"
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>
#include <dlfcn.h>
#include <assert.h>

/* ncurses.h likes to define stuff for us.
 * This unforunately mangles with our struct. */
#undef erase
#undef refresh
#undef mvprintw
#undef move
#undef init_pair
#undef attroff
#undef attron
#undef getmaxx
#undef getmaxy
#undef timeout

static struct curses {
    void *handle;
    WINDOW *stdscr;
    WINDOW* (*initscr)(void);
    int (*endwin)(void);
    int (*refresh)(void);
    int (*erase)(void);
    int (*get_wch)(wint_t *wch);
    int (*mvprintw)(int x, int y, const char *fmt, ...);
    int (*move)(int x, int y);
    int (*init_pair)(short color, short f, short b);
    int (*attroff)(int attrs);
    int (*attron)(int attrs);
    int (*start_color)(void);
    int (*getmaxx)(WINDOW *win);
    int (*getmaxy)(WINDOW *win);
    int (*keypad)(WINDOW *win, bool bf);
    int *ESCDELAY;
} curses;

static void _bmDrawCursesDrawLine(int pair, int y, const char *format, ...)
{
    static int ncols = 0;
    static char *buffer = NULL;
    int newNcols = curses.getmaxx(curses.stdscr);

    if (newNcols <= 0)
        return;

    if (!buffer || newNcols > ncols) {
        if (buffer)
            free(buffer);

        ncols = newNcols;

        if (!(buffer = calloc(1, ncols + 1)))
            return;
    }

    va_list args;
    va_start(args, format);
    int tlen = vsnprintf(NULL, 0, format, args) + 1;
    if (tlen > ncols)
        tlen = ncols;
    va_end(args);

    va_start(args, format);
    vsnprintf(buffer, tlen, format, args);
    va_end(args);

    memset(buffer + tlen - 1, ' ', ncols - tlen + 1);

    if (pair > 0)
        curses.attron(COLOR_PAIR(pair));

    curses.mvprintw(y, 0, buffer);

    if (pair > 0)
        curses.attroff(COLOR_PAIR(pair));
}

static void _bmDrawCursesRender(const bmMenu *menu)
{
    if (!curses.stdscr) {
        freopen("/dev/tty", "rw", stdin);
        setlocale(LC_CTYPE, "");
        if ((curses.stdscr = curses.initscr()) == NULL)
            return;

        *curses.ESCDELAY = 25;
        curses.keypad(curses.stdscr, true);

        curses.start_color();
        curses.init_pair(1, COLOR_BLACK, COLOR_RED);
        curses.init_pair(2, COLOR_RED, COLOR_BLACK);
    }

    const unsigned int lines = curses.getmaxy(curses.stdscr);
    curses.erase();

    size_t titleLen = (menu->title ? strlen(menu->title) + 1 : 0);

    _bmDrawCursesDrawLine(0, 0, "%*s%s", titleLen, "", menu->filter);

    if (menu->title) {
        curses.attron(COLOR_PAIR(1));
        curses.mvprintw(0, 0, menu->title);
        curses.attroff(COLOR_PAIR(1));
    }

    unsigned int i, cl = 1;
    unsigned int itemsCount;
    bmItem **items = bmMenuGetFilteredItems(menu, &itemsCount);
    for (i = (menu->index / (lines - 1)) * (lines - 1); i < itemsCount && cl < lines; ++i) {
        int selected = (items[i] == bmMenuGetSelectedItem(menu));
        _bmDrawCursesDrawLine((selected ? 2 : 0), cl++, "%s%s", (selected ? ">> " : "   "), items[i]->text);
    }

    curses.move(0, titleLen + menu->cursesCursor);
    curses.refresh();
}

static void _bmDrawCursesEndWin(void)
{
    if (curses.refresh)
        curses.refresh();

    if (curses.endwin)
        curses.endwin();

    curses.stdscr = NULL;
}

static bmKey _bmDrawCursesGetKey(unsigned int *unicode)
{
    assert(unicode);
    *unicode = 0;

    if (!curses.stdscr)
        return BM_KEY_NONE;

    curses.get_wch(unicode);
    switch (*unicode) {
        case 16: /* C-p */
        case KEY_UP: return BM_KEY_UP;

        case 14: /* C-n */
        case KEY_DOWN: return BM_KEY_DOWN;

        case 2: /* C-b */
        case KEY_LEFT: return BM_KEY_LEFT;

        case 6: /* C-f */
        case KEY_RIGHT: return BM_KEY_RIGHT;

        case 1: /* C-a */
        case KEY_HOME: return BM_KEY_HOME;

        case 5: /* C-e */
        case KEY_END: return BM_KEY_END;

        case KEY_PPAGE: return BM_KEY_PAGE_UP;
        case KEY_NPAGE: return BM_KEY_PAGE_DOWN;

        case 8: /* C-h */
        case KEY_BACKSPACE: return BM_KEY_BACKSPACE;

        case 4: /* C-d */
        case KEY_DC: return BM_KEY_DELETE;

        case 21: return BM_KEY_LINE_DELETE_LEFT; /* C-u */
        case 11: return BM_KEY_LINE_DELETE_RIGHT; /* C-k */
        case 23: return BM_KEY_WORD_DELETE; /* C-w */

        case 9: return BM_KEY_TAB; /* Tab */

        case 10: /* Return */
            _bmDrawCursesEndWin();
            return BM_KEY_RETURN;

        case 7: /* C-g */
        case 27: /* Escape */
            _bmDrawCursesEndWin();
            return BM_KEY_ESCAPE;

        default: break;
    }

    return BM_KEY_UNICODE;
}

static void _bmDrawCursesFree(void)
{
    _bmDrawCursesEndWin();

    if (curses.handle)
        dlclose(curses.handle);

    memset(&curses, 0, sizeof(curses));
}

int _bmDrawCursesInit(struct _bmRenderApi *api)
{
    memset(&curses, 0, sizeof(curses));

    /* FIXME: hardcoded and not cross-platform */
    curses.handle = dlopen("/usr/lib/libncursesw.so.5", RTLD_LAZY);

    if (!curses.handle)
        return 0;

#define bmLoadFunction(x) (curses.x = dlsym(curses.handle, #x))

    if (!bmLoadFunction(initscr))
        goto function_pointer_exception;
    if (!bmLoadFunction(endwin))
        goto function_pointer_exception;
    if (!bmLoadFunction(refresh))
        goto function_pointer_exception;
    if (!bmLoadFunction(get_wch))
        goto function_pointer_exception;
    if (!bmLoadFunction(erase))
        goto function_pointer_exception;
    if (!bmLoadFunction(mvprintw))
        goto function_pointer_exception;
    if (!bmLoadFunction(move))
        goto function_pointer_exception;
    if (!bmLoadFunction(init_pair))
        goto function_pointer_exception;
    if (!bmLoadFunction(attroff))
        goto function_pointer_exception;
    if (!bmLoadFunction(attron))
        goto function_pointer_exception;
    if (!bmLoadFunction(start_color))
        goto function_pointer_exception;
    if (!bmLoadFunction(getmaxx))
        goto function_pointer_exception;
    if (!bmLoadFunction(getmaxy))
        goto function_pointer_exception;
    if (!bmLoadFunction(keypad))
        goto function_pointer_exception;
    if (!bmLoadFunction(ESCDELAY))
        goto function_pointer_exception;

#undef bmLoadFunction

    api->getKey = _bmDrawCursesGetKey;
    api->render = _bmDrawCursesRender;
    api->free = _bmDrawCursesFree;

    return 1;

function_pointer_exception:
    _bmDrawCursesFree();
    return 0;
}

/* vim: set ts=8 sw=4 tw=0 :*/
