#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/**
 * Filter function map.
 */
static bmItem** (*filterFunc[BM_FILTER_MODE_LAST])(bmMenu *menu, unsigned int *count, unsigned int *selected) = {
    _bmFilterDmenu, /* BM_FILTER_DMENU */
    _bmFilterDmenuCaseInsensitive /* BM_FILTER_DMENU_CASE_INSENSITIVE */
};

static void _bmMenuFilter(bmMenu *menu)
{
    assert(menu);

    if (menu->filteredItems)
        free(menu->filteredItems);

    menu->filteredCount = 0;
    menu->filteredItems = NULL;

    unsigned int count, selected;
    bmItem **filtered = filterFunc[menu->filterMode](menu, &count, &selected);

    menu->filteredItems = filtered;
    menu->filteredCount = count;
    menu->index = selected;
}

static int _bmMenuGrowItems(bmMenu *menu)
{
    void *tmp;
    static const unsigned int step = 32;
    unsigned int nsize = sizeof(bmItem*) * (menu->allocatedCount + step);

    if (!(tmp = realloc(menu->items, nsize))) {
        if (!(tmp = malloc(nsize)))
            return 0;

        memcpy(tmp, menu->items, sizeof(bmItem*) * menu->allocatedCount);
    }

    menu->items = tmp;
    menu->allocatedCount += step;
    memset(&menu->items[menu->itemsCount], 0, sizeof(bmItem*) * (menu->allocatedCount - menu->itemsCount));
    return 1;
}

/**
 * Create new bmMenu instance.
 *
 * @param drawMode Render method to be used for this menu instance.
 * @return bmMenu for new menu instance, NULL if creation failed.
 */
bmMenu* bmMenuNew(bmDrawMode drawMode)
{
    bmMenu *menu = calloc(1, sizeof(bmMenu));

    menu->drawMode = drawMode;

    if (!menu)
        return NULL;

    int status = 1;

    switch (menu->drawMode) {
        case BM_DRAW_MODE_CURSES:
            status = _bmDrawCursesInit(&menu->renderApi);
            break;

        default:break;
    }

    if (status == 0) {
        bmMenuFree(menu);
        return NULL;
    }

    return menu;
}

/**
 * Release bmMenu instance.
 *
 * @param menu bmMenu instance to be freed from memory.
 */
void bmMenuFree(bmMenu *menu)
{
    assert(menu);

    if (menu->renderApi.free)
        menu->renderApi.free();

    if (menu->title)
        free(menu->title);

    if (menu->filteredItems)
        free(menu->filteredItems);

    bmMenuFreeItems(menu);
    free(menu);
}

/**
 * Release items inside bmMenu instance.
 *
 * @param menu bmMenu instance which items will be freed from memory.
 */
void bmMenuFreeItems(bmMenu *menu)
{
    assert(menu);

    unsigned int i;
    for (i = 0; i < menu->itemsCount; ++i)
        bmItemFree(menu->items[i]);

    free(menu->items);
    menu->allocatedCount = menu->itemsCount = 0;
    menu->items = NULL;
}

/**
 * Set active filter mode to bmMenu instance.
 *
 * @param menu bmMenu instance where to set filter mode.
 * @param mode bmFilterMode constant.
 */
void bmMenuSetFilterMode(bmMenu *menu, bmFilterMode mode)
{
    assert(menu);

    bmFilterMode oldMode = mode;
    menu->filterMode = (mode >= BM_FILTER_MODE_LAST ? BM_FILTER_MODE_DMENU : mode);

    if (oldMode != mode)
        _bmMenuFilter(menu);
}

/**
 * Get active filter mode from bmMenu instance.
 *
 * @param menu bmMenu instance where to get filter mode.
 * @return bmFilterMode constant.
 */
bmFilterMode bmMenuGetFilterMode(const bmMenu *menu)
{
    assert(menu);
    return menu->filterMode;
}

/**
 * Set title to bmMenu instance.
 *
 * @param menu bmMenu instance where to set title.
 * @param title C "string" to set as title, can be NULL for empty title.
 */
int bmMenuSetTitle(bmMenu *menu, const char *title)
{
    assert(menu);

    char *copy = NULL;
    if (title && !(copy = _bmStrdup(title)))
        return 0;

    if (menu->title)
        free(menu->title);

    menu->title = copy;
    return 1;
}

/**
 * Get title from bmMenu instance.
 *
 * @param menu bmMenu instance where to get title from.
 * @return Pointer to null terminated C "string", can be NULL for empty title.
 */
const char* bmMenuGetTitle(const bmMenu *menu)
{
    assert(menu);
    return menu->title;
}

/**
 * Add item to bmMenu instance at specific index.
 *
 * @param menu bmMenu instance where item will be added.
 * @param item bmItem instance to add.
 * @param index Index where item will be added.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuAddItemAt(bmMenu *menu, bmItem *item, unsigned int index)
{
    assert(menu);
    assert(item);

    if (menu->itemsCount >= menu->allocatedCount && !_bmMenuGrowItems(menu))
        return 0;

    if (index + 1 != menu->itemsCount) {
        unsigned int i = index;
        memmove(&menu->items[i + 1], &menu->items[i], sizeof(bmItem*) * (menu->itemsCount - i));
    }

    menu->items[index] = item;
    menu->itemsCount++;
    return 1;
}

/**
 * Add item to bmMenu instance.
 *
 * @param menu bmMenu instance where item will be added.
 * @param item bmItem instance to add.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuAddItem(bmMenu *menu, bmItem *item)
{
    return bmMenuAddItemAt(menu, item, menu->itemsCount);
}

/**
 * Remove item from bmMenu instance at specific index.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param index Index of item to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItemAt(bmMenu *menu, unsigned int index)
{
    assert(menu);

    unsigned int i = index;
    if (i >= menu->itemsCount)
        return 0;

    memmove(&menu->items[i], &menu->items[i], sizeof(bmItem*) * (menu->itemsCount - i));
    return 1;
}

/**
 * Remove item from bmMenu instance.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param item bmItem instance to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItem(bmMenu *menu, bmItem *item)
{
    assert(menu);
    assert(item);

    unsigned int i;
    for (i = 0; i < menu->itemsCount && menu->items[i] != item; ++i);
    return bmMenuRemoveItemAt(menu, i);
}

/**
 * Get selected item from bmMenu instance.
 *
 * @param menu bmMenu instance from where to get selected item.
 * @return Selected bmItem instance, NULL if none selected.
 */
bmItem* bmMenuGetSelectedItem(const bmMenu *menu)
{
    assert(menu);

    unsigned int count;
    bmItem **items = bmMenuGetFilteredItems(menu, &count);

    if (!items || count <= menu->index)
        return NULL;

    return items[menu->index];
}

/**
 * Get items from bmMenu instance.
 *
 * @param menu bmMenu instance from where to get items.
 * @param nmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetItems(const bmMenu *menu, unsigned int *nmemb)
{
    assert(menu);

    if (nmemb)
        *nmemb = menu->itemsCount;

    return menu->items;
}

/**
 * Get filtered (displayed) items from bmMenu instance.
 *
 * @warning The pointer returned by this function _will_ be invalid when menu internally filters its list again.
 *          Do not store this pointer.
 *
 * @param menu bmMenu instance from where to get filtered items.
 * @param nmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetFilteredItems(const bmMenu *menu, unsigned int *nmemb)
{
    assert(menu);

    if (nmemb)
        *nmemb = (menu->filteredItems ? menu->filteredCount : menu->itemsCount);

    return (menu->filteredItems ? menu->filteredItems : menu->items);
}

/**
 * Set items to bmMenu instance.
 * Will replace all the old items on bmMenu instance.
 *
 * @param menu bmMenu instance where items will be set.
 * @param items Array of bmItem pointers to set.
 * @param nmemb Total count of items in array.
 * @return 1 on successful set, 0 on failure.
 */
int bmMenuSetItems(bmMenu *menu, const bmItem **items, unsigned int nmemb)
{
    assert(menu);

    if (!items || nmemb == 0) {
        bmMenuFreeItems(menu);
        return 1;
    }

    bmItem **newItems;
    if (!(newItems = calloc(sizeof(bmItem*), nmemb)))
        return 0;

    memcpy(newItems, items, sizeof(bmItem*) * nmemb);
    bmMenuFreeItems(menu);

    menu->items = newItems;
    menu->allocatedCount = menu->itemsCount = nmemb;
    return 1;
}

/**
 * Render bmMenu instance using chosen draw method.
 *
 * @param menu bmMenu instance to be rendered.
 */
void bmMenuRender(const bmMenu *menu)
{
    assert(menu);

    if (menu->renderApi.render)
        menu->renderApi.render(menu);
}

/**
 * Poll key and unicode from underlying UI toolkit.
 *
 * This function will block on CURSES draw mode.
 *
 * @param menu bmMenu instance from which to poll.
 * @param unicode Reference to unsigned int.
 * @return bmKey for polled key.
 */
bmKey bmMenuGetKey(bmMenu *menu, unsigned int *unicode)
{
    assert(menu);
    assert(unicode);

    *unicode = 0;
    bmKey key = BM_KEY_NONE;

    if (menu->renderApi.getKey)
        key = menu->renderApi.getKey(unicode);

    return key;
}

/**
 * Advances menu logic with key and unicode as input.
 *
 * @param menu bmMenu instance to be advanced.
 * @return bmRunResult for menu state.
 */
bmRunResult bmMenuRunWithKey(bmMenu *menu, bmKey key, unsigned int unicode)
{
    assert(menu);
    char *oldFilter = _bmStrdup(menu->filter);
    unsigned int itemsCount = (menu->filteredItems ? menu->filteredCount : menu->itemsCount);

    switch (key) {
        case BM_KEY_LEFT:
            {
                unsigned int oldCursor = menu->cursor;
                menu->cursor -= _bmUtf8RunePrev(menu->filter, menu->cursor);
                menu->cursesCursor -= _bmUtf8RuneWidth(menu->filter + menu->cursor, oldCursor - menu->cursor);
            }
            break;

        case BM_KEY_RIGHT:
            {
                unsigned int oldCursor = menu->cursor;
                menu->cursor += _bmUtf8RuneNext(menu->filter, menu->cursor);
                menu->cursesCursor += _bmUtf8RuneWidth(menu->filter + oldCursor, menu->cursor - oldCursor);
            }
            break;

        case BM_KEY_HOME:
            menu->cursesCursor = menu->cursor = 0;
            break;

        case BM_KEY_END:
            menu->cursor = strlen(menu->filter);
            menu->cursesCursor = _bmUtf8StringScreenWidth(menu->filter);
            break;

        case BM_KEY_UP:
            if (menu->index > 0)
                menu->index--;
            break;

        case BM_KEY_DOWN:
            if (menu->index < itemsCount - 1)
                menu->index++;
            break;

        case BM_KEY_PAGE_UP:
            menu->index = 0;
            break;

        case BM_KEY_PAGE_DOWN:
            menu->index = itemsCount - 1;
            break;

        case BM_KEY_BACKSPACE:
            {
                size_t width;
                menu->cursor -= _bmUtf8RuneRemove(menu->filter, menu->cursor, &width);
                menu->cursesCursor -= width;
            }
            break;

        case BM_KEY_DELETE:
            _bmUtf8RuneRemove(menu->filter, menu->cursor + 1, NULL);
            break;

        case BM_KEY_LINE_DELETE_LEFT:
            {
                while (menu->cursor > 0) {
                    size_t width;
                    menu->cursor -= _bmUtf8RuneRemove(menu->filter, menu->cursor, &width);
                    menu->cursesCursor -= width;
                }
            }
            break;

        case BM_KEY_LINE_DELETE_RIGHT:
            menu->filter[menu->cursor] = 0;
            break;

        case BM_KEY_WORD_DELETE:
            {
                while (menu->cursor < strlen(menu->filter) && !isspace(menu->filter[menu->cursor])) {
                    unsigned int oldCursor = menu->cursor;
                    menu->cursor += _bmUtf8RuneNext(menu->filter, menu->cursor);
                    menu->cursesCursor += _bmUtf8RuneWidth(menu->filter + oldCursor, menu->cursor - oldCursor);
                }
                while (menu->cursor > 0 && isspace(menu->filter[menu->cursor - 1])) {
                    unsigned int oldCursor = menu->cursor;
                    menu->cursor -= _bmUtf8RunePrev(menu->filter, menu->cursor);
                    menu->cursesCursor -= _bmUtf8RuneWidth(menu->filter + menu->cursor, oldCursor - menu->cursor);
                }
                while (menu->cursor > 0 && !isspace(menu->filter[menu->cursor - 1])) {
                    size_t width;
                    menu->cursor -= _bmUtf8RuneRemove(menu->filter, menu->cursor, &width);
                    menu->cursesCursor -= width;
                }
            }
            break;

        case BM_KEY_UNICODE:
            {
                size_t width;
                menu->cursor += _bmUnicodeInsert(menu->filter, sizeof(menu->filter) - 1, menu->cursor, unicode, &width);
                menu->cursesCursor += width;
            }
            break;

        case BM_KEY_TAB:
            {
                bmItem *selected = bmMenuGetSelectedItem(menu);
                if (selected && bmItemGetText(selected)) {
                    const char *text = bmItemGetText(selected);
                    size_t len = strlen(text);

                    if (len > sizeof(menu->filter) - 1)
                        len = sizeof(menu->filter) - 1;

                    memset(menu->filter, 0, strlen(menu->filter));
                    memcpy(menu->filter, text, len);
                    menu->cursor = strlen(menu->filter);
                    menu->cursesCursor = _bmUtf8StringScreenWidth(menu->filter);
                }
            }
            break;

        default: break;
    }

    if (oldFilter && strcmp(oldFilter, menu->filter))
        _bmMenuFilter(menu);

    if (oldFilter)
        free(oldFilter);

    switch (key) {
        case BM_KEY_RETURN: return BM_RUN_RESULT_SELECTED;
        case BM_KEY_ESCAPE: return BM_RUN_RESULT_CANCEL;
        default: break;
    }

    return BM_RUN_RESULT_RUNNING;
}

/* vim: set ts=8 sw=4 tw=0 :*/
