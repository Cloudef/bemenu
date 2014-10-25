#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void
list_free_list(struct list *list)
{
    assert(list);
    free(list->items);
    list->allocated = list->count = 0;
    list->items = NULL;
}

void
list_free_items(struct list *list, list_free_fun destructor)
{
    assert(list);

    for (uint32_t i = 0; i < list->count; ++i)
        destructor(list->items[i]);

    list_free_list(list);
}

void*
list_get_items(const struct list *list, uint32_t *out_nmemb)
{
    assert(list);

    if (out_nmemb)
        *out_nmemb = list->count;

    return list->items;
}

/** !!! Frees the old list, not items !!! */
bool
list_set_items_no_copy(struct list *list, void *items, uint32_t nmemb)
{
    assert(list);

    list_free_list(list);

    if (!items || nmemb == 0) {
        items = NULL;
        nmemb = 0;
    }

    list->items = items;
    list->allocated = list->count = nmemb;
    return true;
}

/** !!! Frees the old items and list !!! */
bool
list_set_items(struct list *list, const void *items, uint32_t nmemb, list_free_fun destructor)
{
    assert(list);

    if (!items || nmemb == 0) {
        list_free_items(list, destructor);
        return true;
    }

    void *new_items;
    if (!(new_items = calloc(sizeof(void*), nmemb)))
        return false;

    memcpy(new_items, items, sizeof(void*) * nmemb);
    return list_set_items_no_copy(list, new_items, nmemb);
}

bool
list_grow(struct list *list, uint32_t step)
{
    assert(list);

    void *tmp;
    uint32_t nsize = sizeof(void*) * (list->allocated + step);

    if (!(tmp = realloc(list->items, nsize)))
        return false;

    list->items = tmp;
    list->allocated += step;
    memset(&list->items[list->count], 0, sizeof(void*) * (list->allocated - list->count));
    return true;
}

bool
list_add_item_at(struct list *list, void *item, uint32_t index)
{
    assert(list && item);

    if ((!list->items || list->allocated <= list->count) && !list_grow(list, 32))
        return false;

    if (index + 1 != list->count) {
        uint32_t i = index;
        memmove(&list->items[i + 1], &list->items[i], sizeof(void*) * (list->count - i));
    }

    list->items[index] = item;
    list->count++;
    return true;
}

bool
list_add_item(struct list *list, void *item)
{
    assert(list);
    return list_add_item_at(list, item, list->count);
}

bool
list_remove_item_at(struct list *list, uint32_t index)
{
    assert(list);

    uint32_t i = index;
    if (!list->items || list->count <= i)
        return false;

    memmove(&list->items[i], &list->items[i + 1], sizeof(void*) * (list->count - i));
    list->count--;
    return true;
}

bool
list_remove_item(struct list *list, const void *item)
{
    assert(list && item);

    uint32_t i;
    for (i = 0; i < list->count && list->items[i] != item; ++i);
    return list_remove_item_at(list, i);
}

void
list_sort(struct list *list, int (*compar)(const void *a, const void *b))
{
    assert(list && compar);
    qsort(list->items, list->count, sizeof(void*), compar);
}

/* vim: set ts=8 sw=4 tw=0 :*/
