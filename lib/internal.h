#include "bemenu.h"

#ifndef size_t
#   include <stddef.h> /* for size_t */
#endif

/**
 * Internal bmItem struct that is not exposed to public.
 * Represents a single item in menu.
 */
struct _bmItem {
    /**
     * Primary text shown on item as null terminated C "string".
     * Matching will be done against this text as well.
     */
    char *text;
};

/**
 * Internal bmRenderApi struct.
 * Renderers should be able to fill this one as they see fit.
 */
struct _bmRenderApi {
    /**
     * If the underlying renderer is a UI toolkit. (curses, etc...)
     * There might be possibility to get user input, and this should be thus implemented.
     */
    bmKey (*getKey)(unsigned int *unicode);

    /**
     * Tells underlying renderer to draw the menu.
     */
    void (*render)(const bmMenu *menu);

    /**
     * Release underlying renderer.
     */
    void (*free)(void);
};

/**
 * Internal bmMenu struct that is not exposed to public.
 */
struct _bmMenu {
    /**
     * Underlying renderer access.
     */
    struct _bmRenderApi renderApi;

    /**
     * All items contained in menu instance.
     */
    struct _bmItem **items;

    /**
     * Filtered/displayed items contained in menu instance.
     */
    struct _bmItem **filteredItems;

    /**
     * Menu instance title.
     */
    char *title;

    /**
     * Text used to filter matches.
     *
     * XXX: Change this to a pointer?
     */
    char filter[1024];

    /**
     * Current byte offset on filter text.
     */
    unsigned int cursor;

    /**
     * Current column/cursor position on filter text.
     */
    unsigned int cursesCursor;

    /**
     * Number of items in menu instance.
     */
    unsigned int itemsCount;

    /**
     * Number of filtered items in menu instance.
     */
    unsigned int filteredCount;

    /**
     * Number of allocated items in menu instance.
     */
    unsigned int allocatedCount;

    /**
     * Current filtered item index in menu instance.
     * This index is valid for the list returned by bmMenuGetFilteredItems.
     */
    unsigned int index;

    /**
     * Current filtering method in menu instance.
     */
    bmFilterMode filterMode;

    /**
     * Drawing mode used in menu instance.
     */
    bmDrawMode drawMode;
};

/* draw/curses.c */
int _bmDrawCursesInit(struct _bmRenderApi *api);

/* filter.c */
bmItem** _bmFilterDmenu(bmMenu *menu, unsigned int *count, unsigned int *selected);
bmItem** _bmFilterDmenuCaseInsensitive(bmMenu *menu, unsigned int *count, unsigned int *selected);

/* util.c */
char* _bmStrdup(const char *s);
bmItem** _bmShrinkItemList(bmItem ***list, size_t osize, size_t nsize);
int _bmUtf8StringScreenWidth(const char *string);
size_t _bmUtf8RuneNext(const char *string, size_t start);
size_t _bmUtf8RunePrev(const char *string, size_t start);
size_t _bmUtf8RuneWidth(const char *rune, unsigned int u8len);
size_t _bmUtf8RuneRemove(char *string, size_t start, size_t *runeWidth);
size_t _bmUtf8RuneInsert(char *string, size_t bufSize, size_t start, const char *rune, unsigned int u8len, size_t *runeWidth);
size_t _bmUnicodeInsert(char *string, size_t bufSize, size_t start, unsigned int unicode, size_t *runeWidth);

/* vim: set ts=8 sw=4 tw=0 :*/
