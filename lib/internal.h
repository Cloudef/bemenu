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
     * Userdata pointer.
     * This pointer will be passed around with the item untouched.
     */
    void *userdata;

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

struct _bmItemList {
    /**
     * Items in the list.
     */
    struct _bmItem **list;

    /**
     * Number of items.
     */
    unsigned int count;

    /**
     * Number of allocated items.
     */
    unsigned int allocated;
};

/**
 * Internal bmMenu struct that is not exposed to public.
 */
struct _bmMenu {
    /**
     * Userdata pointer.
     * This pointer will be passed around with the menu untouched.
     */
    void *userdata;

    /**
     * Underlying renderer access.
     */
    struct _bmRenderApi renderApi;

    /**
     * Items contained in menu instance.
     */
    struct _bmItemList items;

    /**
     * Filtered/displayed items contained in menu instance.
     */
    struct _bmItemList filtered;

    /**
     * Selected items.
     */
    struct _bmItemList selection;

    /**
     * Menu instance title.
     */
    char *title;

    /**
     * Text used to filter matches.
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
     * Current filtered/highlighted item index in menu instance.
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

/* menu.c */
int _bmMenuItemIsSelected(const bmMenu *menu, const bmItem *item);

/* filter.c */
bmItem** _bmFilterDmenu(bmMenu *menu, unsigned int *outNmemb, unsigned int *outHighlighted);
bmItem** _bmFilterDmenuCaseInsensitive(bmMenu *menu, unsigned int *outNmemb, unsigned int *outHighlighted);

/* list.c */
void _bmItemListFreeList(struct _bmItemList *list);
void _bmItemListFreeItems(struct _bmItemList *list);
bmItem** _bmItemListGetItems(const struct _bmItemList *list, unsigned int *outNmemb);
int _bmItemListSetItemsNoCopy(struct _bmItemList *list, bmItem **items, unsigned int nmemb);
int _bmItemListSetItems(struct _bmItemList *list, const bmItem **items, unsigned int nmemb);
int _bmItemListGrow(struct _bmItemList *list, unsigned int step);
int _bmItemListAddItemAt(struct _bmItemList *list, bmItem *item, unsigned int index);
int _bmItemListAddItem(struct _bmItemList *list, bmItem *item);
int _bmItemListRemoveItemAt(struct _bmItemList *list, unsigned int index);
int _bmItemListRemoveItem(struct _bmItemList *list, const bmItem *item);

/* util.c */
char* _bmStrdup(const char *s);
int _bmStrupcmp(const char *hay, const char *needle);
char* _bmStrupstr(const char *hay, const char *needle);
bmItem** _bmShrinkItemList(bmItem ***inOutList, size_t osize, size_t nsize);
int _bmUtf8StringScreenWidth(const char *string);
size_t _bmUtf8RuneNext(const char *string, size_t start);
size_t _bmUtf8RunePrev(const char *string, size_t start);
size_t _bmUtf8RuneWidth(const char *rune, unsigned int u8len);
size_t _bmUtf8RuneRemove(char *string, size_t start, size_t *outRuneWidth);
size_t _bmUtf8RuneInsert(char *string, size_t bufSize, size_t start, const char *rune, unsigned int u8len, size_t *outRuneWidth);
size_t _bmUnicodeInsert(char *string, size_t bufSize, size_t start, unsigned int unicode, size_t *outRuneWidth);

/* vim: set ts=8 sw=4 tw=0 :*/
