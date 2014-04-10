#include "internal.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * Allocate a new item.
 *
 * @param text Pointer to null terminated C "string", can be **NULL** for empty text.
 * @return bmItem for new item instance, **NULL** if creation failed.
 */
bmItem* bmItemNew(const char *text)
{
    bmItem *item = calloc(1, sizeof(bmItem));

    if (!item)
        return NULL;

    bmItemSetText(item, text);
    return item;
}

/**
 * Release bmItem instance.
 *
 * @param item bmItem instance to be freed from memory.
 */
void bmItemFree(bmItem *item)
{
    assert(item);

    if (item->text)
        free(item->text);

    free(item);
}

/**
 * Set text to bmItem instance.
 *
 * @param item bmItem instance where to set text.
 * @param text C "string" to set as text, can be **NULL** for empty text.
 */
int bmItemSetText(bmItem *item, const char *text)
{
    assert(item);

    char *copy = NULL;
    if (text && !(copy = _bmStrdup(text)))
        return 0;

    if (item->text)
        free(item->text);

    item->text = copy;
    return 1;
}

/**
 * Get text from bmItem instance.
 *
 * @param item bmItem instance where to get text from.
 * @return Pointer to null terminated C "string", can be **NULL** for empty text.
 */
const char* bmItemGetText(const bmItem *item)
{
    assert(item);
    return item->text;
}

/* vim: set ts=8 sw=4 tw=0 :*/
