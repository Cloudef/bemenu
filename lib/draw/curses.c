#include "../internal.h"

#if __APPLE__
#   define _C99_SOURCE
#   include <stdio.h> /* vsnprintf */
#   undef _C99_SOURCE
#endif

#define _XOPEN_SOURCE 500
#include <signal.h> /* sigaction */
#include <stdarg.h> /* vsnprintf */
#undef _XOPEN_SOURCE

#include <wchar.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>
#include <dlfcn.h>
#include <assert.h>

#if _WIN32
static const char *TTY = "CON";
#else
static const char *TTY = "/dev/tty";
#endif

#if __APPLE__
    const char *DL_PATH[] = {
        "libncursesw.5.dylib",
        "libncurses.5.dylib",
        NULL
    };
#elif _WIN32
#   error FIXME: Compile with windows... or use relative path?
#else
    const char *DL_PATH[] = {
        "libncursesw.so.5",
        "libncurses.so.5",
        NULL
    };
#endif

/* these are implemented as macros in older curses */
#ifndef NCURSES_OPAQUE
static int wrap_getmaxx(WINDOW *win) { return getmaxx(win); }
static int wrap_getmaxy(WINDOW *win) { return getmaxy(win); }
#endif

/* ncurses.h likes to define stuff for us.
 * This unforunately mangles with our struct. */
#undef erase
#undef getch
#undef get_wch
#undef refresh
#undef mvprintw
#undef move
#undef init_pair
#undef attroff
#undef attron
#undef getmaxx
#undef getmaxy

/**
 * Dynamically loaded curses API.
 */
static struct curses {
    struct sigaction abrtAction;
    struct sigaction segvAction;
    struct sigaction winchAction;
    void *handle;
    WINDOW *stdscr;
    WINDOW* (*initscr)(void);
    int (*endwin)(void);
    int (*refresh)(void);
    int (*erase)(void);
    int (*getch)(void);
    int (*get_wch)(wint_t *wch);
    int (*mvprintw)(int x, int y, const char *fmt, ...);
    int (*move)(int x, int y);
    int (*init_pair)(short color, short f, short b);
    int (*attroff)(int attrs);
    int (*attron)(int attrs);
    int (*start_color)(void);
    int (*use_default_colors)(void);
    int (*getmaxx)(WINDOW *win);
    int (*getmaxy)(WINDOW *win);
    int (*keypad)(WINDOW *win, bool bf);
    int (*curs_set)(int visibility);
    int (*flushinp)(void);
    int (*noecho)(void);
    int (*raw)(void);
    int *ESCDELAY;
    int oldStdin;
    int oldStdout;
} curses;

static int _bmDrawCursesResizeBuffer(char **buffer, size_t *osize, size_t nsize)
{
    assert(buffer);
    assert(osize);

    if (nsize == 0 || nsize <= *osize)
        return 0;

    void *tmp;
    if (!*buffer || !(tmp = realloc(*buffer, nsize))) {
        if (!(tmp = malloc(nsize)))
            return 0;

        if (*buffer) {
            memcpy(tmp, *buffer, *osize);
            free(*buffer);
        }
    }

    *buffer = tmp;
    *osize = nsize;
    return 1;
}

#if __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
static void _bmDrawCursesDrawLine(int pair, int y, const char *format, ...)
{
    static size_t blen = 0;
    static char *buffer = NULL;

    size_t ncols = curses.getmaxx(curses.stdscr);
    if (ncols <= 0)
        return;

    va_list args;
    va_start(args, format);
    size_t nlen = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if ((!buffer || nlen > blen) && !_bmDrawCursesResizeBuffer(&buffer, &blen, nlen + 1))
        return;

    va_start(args, format);
    vsnprintf(buffer, blen - 1, format, args);
    va_end(args);

    size_t dw = 0, i = 0;
    while (dw < ncols && i < nlen) {
        if (buffer[i] == '\t') buffer[i] = ' ';
        int next = _bmUtf8RuneNext(buffer, i);
        dw += _bmUtf8RuneWidth(buffer + i, next);
        i += (next ? next : 1);
    }

    if (dw < ncols) {
        /* line is too short, widen it */
        size_t offset = i + (ncols - dw);
        if (blen <= offset && !_bmDrawCursesResizeBuffer(&buffer, &blen, offset + 1))
             return;

        memset(buffer + nlen, ' ', offset - nlen);
        buffer[offset] = 0;
    } else if (i < blen) {
        /* line is too long, shorten it */
        i -= _bmUtf8RunePrev(buffer, i - (dw - ncols)) - 1;
        size_t cc = dw - (dw - ncols);

        size_t offset = i - (dw - ncols) + (ncols - cc) + 1;
        if (blen <= offset) {
            int diff = offset - blen + 1;
            if (!_bmDrawCursesResizeBuffer(&buffer, &blen, blen + diff))
                return;
        }

        memset(buffer + i - (dw - ncols), ' ', (ncols - cc) + 1);
        buffer[offset] = 0;
    }

    if (pair > 0)
        curses.attron(COLOR_PAIR(pair));

    curses.mvprintw(y, 0, "%s", buffer);

    if (pair > 0)
        curses.attroff(COLOR_PAIR(pair));
}

static void _bmDrawCursesRender(const bmMenu *menu)
{
    if (!curses.stdscr) {
        curses.oldStdin = dup(STDIN_FILENO);
        curses.oldStdout = dup(STDOUT_FILENO);

        freopen(TTY, "w", stdout);
        freopen(TTY, "r", stdin);

        setlocale(LC_CTYPE, "");

        if ((curses.stdscr = curses.initscr()) == NULL)
            return;

        *curses.ESCDELAY = 25;
        curses.flushinp();
        curses.keypad(curses.stdscr, true);
        curses.curs_set(1);
        curses.noecho();
        curses.raw();

        curses.start_color();
        curses.use_default_colors();
        curses.init_pair(1, COLOR_BLACK, COLOR_RED);
        curses.init_pair(2, COLOR_RED, -1);
    }

    const unsigned int lines = curses.getmaxy(curses.stdscr);
    curses.erase();

    unsigned int ncols = curses.getmaxx(curses.stdscr);
    unsigned int titleLen = (menu->title ? strlen(menu->title) + 1 : 0);

    if (titleLen >= ncols)
        titleLen = 0;

    unsigned int ccols = ncols - titleLen - 1;
    unsigned int dcols = 0, doffset = menu->cursor;

    while (doffset > 0 && dcols < ccols) {
        int prev = _bmUtf8RunePrev(menu->filter, doffset);
        dcols += _bmUtf8RuneWidth(menu->filter + doffset - prev, prev);
        doffset -= (prev ? prev : 1);
    }

    _bmDrawCursesDrawLine(0, 0, "%*s%s", titleLen, "", (menu->filter ? menu->filter + doffset : ""));

    if (menu->title && titleLen > 0) {
        curses.attron(COLOR_PAIR(1));
        curses.mvprintw(0, 0, menu->title);
        curses.attroff(COLOR_PAIR(1));
    }

    unsigned int i, cl = 1;
    unsigned int itemsCount;
    bmItem **items = bmMenuGetFilteredItems(menu, &itemsCount);
    for (i = (menu->index / (lines - 1)) * (lines - 1); i < itemsCount && cl < lines; ++i) {
        int highlighted = (items[i] == bmMenuGetHighlightedItem(menu));
        int color = (highlighted ? 2 : (_bmMenuItemIsSelected(menu, items[i]) ? 1 : 0));
        _bmDrawCursesDrawLine(color, cl++, "%s%s", (highlighted ? ">> " : "   "), (items[i]->text ? items[i]->text : ""));
    }

    curses.move(0, titleLen + (menu->cursesCursor < ccols ? menu->cursesCursor : ccols));
    curses.refresh();
}

static unsigned int _bmDrawCursesDisplayedCount(const bmMenu *menu)
{
    (void)menu;
    return (curses.stdscr ? curses.getmaxy(curses.stdscr) : 0);
}

static void _bmDrawCursesEndWin(void)
{
    if (!curses.stdscr)
        return;

    freopen(TTY, "w", stdout);

    if (curses.refresh)
        curses.refresh();

    if (curses.endwin)
        curses.endwin();

    dup2(curses.oldStdin, STDIN_FILENO);
    dup2(curses.oldStdout, STDOUT_FILENO);
    close(curses.oldStdin);
    close(curses.oldStdout);

    curses.stdscr = NULL;
}

static bmKey _bmDrawCursesGetKey(unsigned int *unicode)
{
    assert(unicode);
    *unicode = 0;

    if (!curses.stdscr)
        return BM_KEY_NONE;

    if (curses.get_wch)
        curses.get_wch((wint_t*)unicode);
    else if (curses.getch)
        *unicode = curses.getch();

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

        case 18: /* C-r */
            return BM_KEY_CONTROL_RETURN;

        case 20: /* C-t */
        case 331: /* Insert */
            _bmDrawCursesEndWin();
            return BM_KEY_SHIFT_RETURN;

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

    sigaction(SIGABRT, &curses.abrtAction, NULL);
    sigaction(SIGSEGV, &curses.segvAction, NULL);
    sigaction(SIGWINCH, &curses.winchAction, NULL);
    memset(&curses, 0, sizeof(curses));
}

static void _bmDrawCursesCrashHandler(int sig)
{
    (void)sig;
    _bmDrawCursesFree();
}

static void _bmDrawCursesResizeHandler(int sig)
{
    (void)sig;
    if (!curses.stdscr)
        return;

    curses.endwin();
    curses.refresh();
}

int _bmDrawCursesInit(struct _bmRenderApi *api)
{
    memset(&curses, 0, sizeof(curses));
    const char *lib = NULL, *func = NULL;

    unsigned int i;
    for (i = 0; DL_PATH[i] && !curses.handle; ++i)
        curses.handle = dlopen((lib = DL_PATH[i]), RTLD_LAZY);

    if (!curses.handle)
        return 0;

#define bmLoadFunction(x) (curses.x = dlsym(curses.handle, (func = #x)))

    if (!bmLoadFunction(initscr))
        goto function_pointer_exception;
    if (!bmLoadFunction(endwin))
        goto function_pointer_exception;
    if (!bmLoadFunction(refresh))
        goto function_pointer_exception;
    if (!bmLoadFunction(get_wch) && !bmLoadFunction(getch))
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
    if (!bmLoadFunction(use_default_colors))
        goto function_pointer_exception;
    if (!bmLoadFunction(keypad))
        goto function_pointer_exception;
    if (!bmLoadFunction(curs_set))
        goto function_pointer_exception;
    if (!bmLoadFunction(flushinp))
        goto function_pointer_exception;
    if (!bmLoadFunction(noecho))
        goto function_pointer_exception;
    if (!bmLoadFunction(raw))
        goto function_pointer_exception;
    if (!bmLoadFunction(ESCDELAY))
        goto function_pointer_exception;

#ifndef NCURSES_OPAQUE
    curses.getmaxx = wrap_getmaxx;
    curses.getmaxy = wrap_getmaxy;
#else
    if (!bmLoadFunction(getmaxx))
        goto function_pointer_exception;
    if (!bmLoadFunction(getmaxy))
        goto function_pointer_exception;
#endif

#undef bmLoadFunction

    api->displayedCount = _bmDrawCursesDisplayedCount;
    api->getKey = _bmDrawCursesGetKey;
    api->render = _bmDrawCursesRender;
    api->free = _bmDrawCursesFree;

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = _bmDrawCursesCrashHandler;
    sigaction(SIGABRT, &action, &curses.abrtAction);
    sigaction(SIGSEGV, &action, &curses.segvAction);

    action.sa_handler = _bmDrawCursesResizeHandler;
    sigaction(SIGWINCH, &action, &curses.winchAction);
    return 1;

function_pointer_exception:
    fprintf(stderr, "-!- Could not load function '%s' from '%s'\n", func, lib);
    _bmDrawCursesFree();
    return 0;
}

/* vim: set ts=8 sw=4 tw=0 :*/
