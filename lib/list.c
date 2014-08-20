#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void _bmItemListFreeList(struct _bmItemList *list)
{
    assert(list);

    if (list->list)
        free(list->list);

    list->allocated = list->count = 0;
    list->list = NULL;
}

void _bmItemListFreeItems(struct _bmItemList *list)
{
    assert(list);

    unsigned int i;
    for (i = 0; i < list->count; ++i)
        bmItemFree(list->list[i]);

    _bmItemListFreeList(list);
}

bmItem** _bmItemListGetItems(const struct _bmItemList *list, unsigned int *outNmemb)
{
    assert(list);

    if (outNmemb)
        *outNmemb = list->count;

    return list->list;
}

/** !!! Frees the old list, not items !!! */
int _bmItemListSetItemsNoCopy(struct _bmItemList *list, bmItem **items, unsigned int nmemb)
{
    assert(list);

    _bmItemListFreeList(list);

    if (!items || nmemb == 0) {
        items = NULL;
        nmemb = 0;
    }

    list->list = items;
    list->allocated = list->count = nmemb;
    return 1;
}

/** !!! Frees the old items and list !!! */
int _bmItemListSetItems(struct _bmItemList *list, const bmItem **items, unsigned int nmemb)
{
    assert(list);

    if (!items || nmemb == 0) {
        _bmItemListFreeItems(list);
        return 1;
    }

    bmItem **newItems;
    if (!(newItems = calloc(sizeof(bmItem*), nmemb)))
        return 0;

    memcpy(newItems, items, sizeof(bmItem*) * nmemb);
    return _bmItemListSetItemsNoCopy(list, newItems, nmemb);
}

int _bmItemListGrow(struct _bmItemList *list, unsigned int step)
{
    assert(list);

    void *tmp;
    unsigned int nsize = sizeof(bmItem*) * (list->allocated + step);

    if (!list->list || !(tmp = realloc(list->list, nsize))) {
        if (!(tmp = malloc(nsize)))
            return 0;

        if (list->list) {
            memcpy(tmp, list->list, sizeof(bmItem*) * list->allocated);
            free(list->list);
        }
    }

    list->list = tmp;
    list->allocated += step;
    memset(&list->list[list->count], 0, sizeof(bmItem*) * (list->allocated - list->count));
    return 1;
}

int _bmItemListAddItemAt(struct _bmItemList *list, bmItem *item, unsigned int index)
{
    assert(list);
    assert(item);

    if ((!list->list || list->allocated <= list->count) && !_bmItemListGrow(list, 32))
        return 0;

    if (index + 1 != list->count) {
        unsigned int i = index;
        memmove(&list->list[i + 1], &list->list[i], sizeof(bmItem*) * (list->count - i));
    }

    list->list[index] = item;
    list->count++;
    return 1;
}

int _bmItemListAddItem(struct _bmItemList *list, bmItem *item)
{
    assert(list);
    return _bmItemListAddItemAt(list, item, list->count);
}

int _bmItemListRemoveItemAt(struct _bmItemList *list, unsigned int index)
{
    assert(list);

    unsigned int i = index;
    if (!list->list || list->count <= i)
        return 0;

    memmove(&list->list[i], &list->list[i], sizeof(bmItem*) * (list->count - i));
    return 1;
}

int _bmItemListRemoveItem(struct _bmItemList *list, const bmItem *item)
{
    unsigned int i;
    for (i = 0; i < list->count && list->list[i] != item; ++i);
    return _bmItemListRemoveItemAt(list, i);
}

/* vim: set ts=8 sw=4 tw=0 :*/
