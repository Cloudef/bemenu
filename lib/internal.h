#include "bemenu.h"

#ifndef size_t
#   include <stddef.h> /* for size_t */
#endif

/**
 * Internal bmItem struct that is not exposed to public.
 * Represents a single item in menu.
 */
struct _bmItem {
    char *text;
};

/**
 * Internal bmRenderApi struct.
 * Renderers should be able to fill this one as they see fit.
 */
struct _bmRenderApi {
    bmKey (*getKey)(unsigned int *unicode);
    void (*render)(const bmMenu *menu);
    void (*free)(void);
};

/**
 * Internal bmMenu struct that is not exposed to public.
 */
struct _bmMenu {
    struct _bmRenderApi renderApi;
    struct _bmItem **items, **filteredItems;
    char *title, filter[1024];
    unsigned int cursor, cursesCursor;
    unsigned int itemsCount, allocatedCount;
    unsigned int filteredCount;
    unsigned int index;
    bmFilterMode filterMode;
    bmDrawMode drawMode;
};

/* draw/curses.c */
int _bmDrawCursesInit(struct _bmRenderApi *api);

/* menu.c */
int _bmMenuShouldRenderItem(const bmMenu *menu, const bmItem *item);

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
