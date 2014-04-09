/**
 * @file filter.c
 */

#include "internal.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * Filter that mimics the vanilla dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param count unsigned int reference to filtered items count.
 * @param selected unsigned int reference to new selected item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenu(bmMenu *menu, unsigned int *count, unsigned int *selected)
{
    assert(menu != NULL);
    assert(count != NULL);
    assert(selected != NULL);
    *count = *selected = 0;

    /* FIXME: not real dmenu like filtering at all */

    bmItem **filtered = calloc(menu->itemsCount, sizeof(bmItem*));
    if (!filtered)
        return NULL;

    unsigned int i, f;
    for (f = i = 0; i < menu->itemsCount; ++i) {
        bmItem *item = menu->items[i];
        if (item->text && strstr(item->text, menu->filter)) {
            if (f == 0 || item == bmMenuGetSelectedItem(menu))
                *selected = f;
            filtered[f++] = item;
        }
    }

    return _bmShrinkItemList(&filtered, menu->itemsCount, (*count = f));
}

/**
 * Filter that mimics the vanilla case-insensitive dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param count unsigned int reference to filtered items count.
 * @param selected unsigned int reference to new selected item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenuCaseInsensitive(bmMenu *menu, unsigned int *count, unsigned int *selected)
{
    assert(menu != NULL);
    assert(count != NULL);
    assert(selected != NULL);
    *count = *selected = 0;

    /* FIXME: stub */

    return NULL;
}

/* vim: set ts=8 sw=4 tw=0 :*/
