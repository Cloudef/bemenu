#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/**
 * Filter function map.
 */
static bmItem** (*filterFunc[BM_FILTER_MODE_LAST])(bmMenu *menu, char addition, unsigned int *outNmemb, unsigned int *outHighlighted) = {
    _bmFilterDmenu, /* BM_FILTER_DMENU */
    _bmFilterDmenuCaseInsensitive /* BM_FILTER_DMENU_CASE_INSENSITIVE */
};

int _bmMenuItemIsSelected(const bmMenu *menu, const bmItem *item)
{
    assert(menu);
    assert(item);

    unsigned int i, count;
    bmItem **items = bmMenuGetSelectedItems(menu, &count);
    for (i = 0; i < count && items[i] != item; ++i);
    return (i < count);
}

/**
 * Create new bmMenu instance.
 *
 * @param drawMode Render method to be used for this menu instance.
 * @return bmMenu for new menu instance, **NULL** if creation failed.
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

    if (menu->oldFilter)
        free(menu->oldFilter);

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
    _bmItemListFreeList(&menu->selection);
    _bmItemListFreeList(&menu->filtered);
    _bmItemListFreeItems(&menu->items);
}

/**
 * Set userdata pointer to bmMenu instance.
 * Userdata will be carried unmodified by the instance.
 *
 * @param menu bmMenu instance where to set userdata pointer.
 * @param userdata Pointer to userdata.
 */
void bmMenuSetUserdata(bmMenu *menu, void *userdata)
{
    assert(menu);
    menu->userdata = userdata;
}

/**
 * Get userdata pointer from bmMenu instance.
 *
 * @param menu bmMenu instance which userdata pointer to get.
 * @return Pointer for unmodified userdata.
 */
void* bmMenuGetUserdata(bmMenu *menu)
{
    assert(menu);
    return menu->userdata;
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
    menu->filterMode = (mode >= BM_FILTER_MODE_LAST ? BM_FILTER_MODE_DMENU : mode);
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
 * @param title C "string" to set as title, can be **NULL** for empty title.
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
 * @return Pointer to null terminated C "string", can be **NULL** for empty title.
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
    return _bmItemListAddItemAt(&menu->items, item, index);
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
    return _bmItemListAddItem(&menu->items, item);
}

/**
 * Remove item from bmMenu instance at specific index.
 *
 * @warning The item won't be freed, use bmItemFree to do that.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param index Index of item to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItemAt(bmMenu *menu, unsigned int index)
{
    assert(menu);

    if (!menu->items.list || menu->items.count <= index)
        return 0;

    bmItem *item = menu->items.list[index];
    int ret = _bmItemListRemoveItemAt(&menu->items, index);

    if (ret) {
        _bmItemListRemoveItem(&menu->selection, item);
        _bmItemListRemoveItem(&menu->filtered, item);
    }

    return ret;
}

/**
 * Remove item from bmMenu instance.
 *
 * @warning The item won't be freed, use bmItemFree to do that.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param item bmItem instance to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItem(bmMenu *menu, bmItem *item)
{
    assert(menu);

    int ret = _bmItemListRemoveItem(&menu->items, item);

    if (ret) {
        _bmItemListRemoveItem(&menu->selection, item);
        _bmItemListRemoveItem(&menu->filtered, item);
    }

    return ret;
}

/**
 * Highlight item in menu by index.
 *
 * @param menu bmMenu instance from where to highlight item.
 * @param index Index of item to highlight.
 * @return 1 on successful highlight, 0 on failure.
 */
int bmMenuSetHighlightedIndex(bmMenu *menu, unsigned int index)
{
    assert(menu);

    unsigned int itemsCount;
    bmMenuGetFilteredItems(menu, &itemsCount);

    if (itemsCount <= index)
        return 0;

    return (menu->index = index);
}

/**
 * Highlight item in menu.
 *
 * @param menu bmMenu instance from where to highlight item.
 * @param item bmItem instance to highlight.
 * @return 1 on successful highlight, 0 on failure.
 */
int bmMenuSetHighlighted(bmMenu *menu, bmItem *item)
{
    assert(menu);

    unsigned int i, itemsCount;
    bmItem **items = bmMenuGetFilteredItems(menu, &itemsCount);
    for (i = 0; i < itemsCount && items[i] != item; ++i);

    if (itemsCount <= i)
        return 0;

    return (menu->index = i);
}

/**
 * Get highlighted item from bmMenu instance.
 *
 * @warning The pointer returned by this function may be invalid after items change.
 *
 * @param menu bmMenu instance from where to get highlighted item.
 * @return Selected bmItem instance, **NULL** if none highlighted.
 */
bmItem* bmMenuGetHighlightedItem(const bmMenu *menu)
{
    assert(menu);

    unsigned int count;
    bmItem **items = bmMenuGetFilteredItems(menu, &count);

    if (!items || count <= menu->index)
        return NULL;

    return items[menu->index];
}

/**
 * Set selected items to bmMenu instance.
 *
 * @warning The list won't be copied, do not free it.
 *
 * @param menu bmMenu instance where items will be set.
 * @param items Array of bmItem pointers to set.
 * @param nmemb Total count of items in array.
 * @return 1 on successful set, 0 on failure.
 */
int bmMenuSetSelectedItems(bmMenu *menu, bmItem **items, unsigned int nmemb)
{
    assert(menu);
    return _bmItemListSetItemsNoCopy(&menu->selection, items, nmemb);
}

/**
 * Get selected items from bmMenu instance.
 *
 * @warning The pointer returned by this function may be invalid after selection or items change.
 *
 * @param menu bmMenu instance from where to get selected items.
 * @param outNmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetSelectedItems(const bmMenu *menu, unsigned int *outNmemb)
{
    assert(menu);
    return _bmItemListGetItems(&menu->selection, outNmemb);
}

/**
 * Set items to bmMenu instance.
 * Will replace all the old items on bmMenu instance.
 *
 * If items is **NULL**, or nmemb is zero, all items will be freed from the menu.
 *
 * @param menu bmMenu instance where items will be set.
 * @param items Array of bmItem pointers to set.
 * @param nmemb Total count of items in array.
 * @return 1 on successful set, 0 on failure.
 */
int bmMenuSetItems(bmMenu *menu, const bmItem **items, unsigned int nmemb)
{
    assert(menu);

    int ret = _bmItemListSetItems(&menu->items, items, nmemb);

    if (ret) {
        _bmItemListFreeList(&menu->selection);
        _bmItemListFreeList(&menu->filtered);
    }

    return ret;
}

/**
 * Get items from bmMenu instance.
 *
 * @warning The pointer returned by this function may be invalid after removing or adding new items.
 *
 * @param menu bmMenu instance from where to get items.
 * @param outNmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetItems(const bmMenu *menu, unsigned int *outNmemb)
{
    assert(menu);
    return _bmItemListGetItems(&menu->items, outNmemb);
}

/**
 * Get filtered (displayed) items from bmMenu instance.
 *
 * @warning The pointer returned by this function _will_ be invalid when menu internally filters its list again.
 *          Do not store this pointer.
 *
 * @param menu bmMenu instance from where to get filtered items.
 * @param outNmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetFilteredItems(const bmMenu *menu, unsigned int *outNmemb)
{
    assert(menu);

    if (strlen(menu->filter))
        return _bmItemListGetItems(&menu->filtered, outNmemb);

    return _bmItemListGetItems(&menu->items, outNmemb);
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
 * Trigger filtering of menu manually.
 * This is useful when adding new items and want to dynamically see them filtered.
 *
 * Do note that filtering might be heavy, so you should only call it after batch manipulation of items.
 * Not after manipulation of each single item.
 *
 * @param menu bmMenu instance which to filter.
 */
void bmMenuFilter(bmMenu *menu)
{
    assert(menu);

    char addition = 0;
    size_t len = strlen(menu->filter);

    if (!len || !menu->items.list || menu->items.count <= 0) {
        _bmItemListFreeList(&menu->filtered);

        if (menu->oldFilter)
            free(menu->oldFilter);

        menu->oldFilter = NULL;
        return;
    }

    if (menu->oldFilter) {
        size_t oldLen = strlen(menu->oldFilter);
        addition = (oldLen < len && !memcmp(menu->oldFilter, menu->filter, oldLen));
    }

    if (menu->oldFilter && addition && menu->filtered.count <= 0)
        return;

    if (menu->oldFilter && !strcmp(menu->filter, menu->oldFilter))
        return;

    unsigned int count, selected;
    bmItem **filtered = filterFunc[menu->filterMode](menu, addition, &count, &selected);

    _bmItemListSetItemsNoCopy(&menu->filtered, filtered, count);
    menu->index = selected;

    if (count) {
        if (menu->oldFilter)
            free(menu->oldFilter);

        menu->oldFilter = _bmStrdup(menu->filter);
    }
}

/**
 * Poll key and unicode from underlying UI toolkit.
 *
 * This function will block on @link ::bmDrawMode BM_DRAW_MODE_CURSES @endlink draw mode.
 *
 * @param menu bmMenu instance from which to poll.
 * @param outUnicode Reference to unsigned int.
 * @return bmKey for polled key.
 */
bmKey bmMenuGetKey(bmMenu *menu, unsigned int *outUnicode)
{
    assert(menu);
    assert(outUnicode);

    *outUnicode = 0;
    bmKey key = BM_KEY_NONE;

    if (menu->renderApi.getKey)
        key = menu->renderApi.getKey(outUnicode);

    return key;
}

/**
 * Advances menu logic with key and unicode as input.
 *
 * @param menu bmMenu instance to be advanced.
 * @param key Key input that will advance menu logic.
 * @param unicode Unicode input that will advance menu logic.
 * @return bmRunResult for menu state.
 */
bmRunResult bmMenuRunWithKey(bmMenu *menu, bmKey key, unsigned int unicode)
{
    assert(menu);

    unsigned int itemsCount;
    bmMenuGetFilteredItems(menu, &itemsCount);

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
                bmItem *highlighted = bmMenuGetHighlightedItem(menu);
                if (highlighted && bmItemGetText(highlighted)) {
                    const char *text = bmItemGetText(highlighted);
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

        case BM_KEY_SHIFT_RETURN:
        case BM_KEY_RETURN:
            {
                bmItem *highlighted = bmMenuGetHighlightedItem(menu);
                if (highlighted && !_bmMenuItemIsSelected(menu, highlighted))
                    _bmItemListAddItem(&menu->selection, highlighted);
            }
            break;

        case BM_KEY_ESCAPE:
            _bmItemListFreeList(&menu->selection);
            break;

        default: break;
    }

    bmMenuFilter(menu);

    switch (key) {
        case BM_KEY_RETURN: return BM_RUN_RESULT_SELECTED;
        case BM_KEY_ESCAPE: return BM_RUN_RESULT_CANCEL;
        default: break;
    }

    return BM_RUN_RESULT_RUNNING;
}

/* vim: set ts=8 sw=4 tw=0 :*/
