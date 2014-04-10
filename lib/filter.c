#include "internal.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * Shrink bmItem** list pointer.
 *
 * Useful helper function for filter functions.
 *
 * @param list Pointer to pointer to list of bmItem pointers.
 * @param osize Current size of the list.
 * @param nsize New size the list will be shrinked to.
 * @return Pointer to list of bmItem pointers.
 */
static bmItem** _bmFilterShrinkList(bmItem ***inOutList, size_t osize, size_t nsize)
{
    assert(inOutList);

    if (nsize == 0) {
        free(*inOutList);
        return (*inOutList = NULL);
    }

    if (nsize >= osize)
        return *inOutList;

    void *tmp = malloc(sizeof(bmItem*) * nsize);
    if (!tmp)
        return *inOutList;

    memcpy(tmp, *inOutList, sizeof(bmItem*) * nsize);
    free(*inOutList);
    return (*inOutList = tmp);
}

/**
 * Text filter tokenizer helper.
 *
 * @param menu bmMenu instance which filter to tokenize.
 * @param outTokv char pointer reference to list of tokens, this should be freed after use.
 * @param outTokc unsigned int reference to number of tokens.
 * @return Pointer to buffer that contains tokenized string, this should be freed after use.
 */
static char* _bmFilterTokenize(bmMenu *menu, char ***outTokv, unsigned int *outTokc)
{
    assert(menu);
    assert(outTokv);
    assert(outTokc);
    *outTokv = NULL;
    *outTokc = 0;

    char **tokv = NULL, *buffer = NULL;
    if (!(buffer = _bmStrdup(menu->filter)))
        goto fail;

    char *s, **tmp = NULL;
    unsigned int tokc = 0, tokn = 0;
    for (s = strtok(buffer, " "); s; tmp[tokc - 1] = s, s = strtok(NULL, " "), tokv = tmp)
        if (++tokc > tokn && !(tmp = realloc(tmp, ++tokn * sizeof(char*))))
            goto fail;

    *outTokv = tmp;
    *outTokc = tokc;
    return buffer;

fail:
    if (buffer)
        free(buffer);
    if (tokv)
        free(tokv);
    return NULL;
}

/**
 * Dmenu filterer that accepts substring function.
 *
 * @param menu bmMenu instance to filter.
 * @param fstrstr Substring function used to match items.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenuFun(bmMenu *menu, char* (*fstrstr)(const char *a, const char *b), unsigned int *outNmemb, unsigned int *outHighlighted)
{
    assert(menu);
    assert(outNmemb);
    assert(outHighlighted);
    *outNmemb = *outHighlighted = 0;

    unsigned int itemsCount;
    bmItem **items = bmMenuGetItems(menu, &itemsCount);

    bmItem **filtered = calloc(itemsCount, sizeof(bmItem*));
    if (!filtered)
        return NULL;

    char **tokv;
    unsigned int tokc;
    char *buffer = _bmFilterTokenize(menu, &tokv, &tokc);

    unsigned int i, f;
    for (f = i = 0; i < itemsCount; ++i) {
        bmItem *item = items[i];
        if (!item->text && tokc != 0)
            continue;

        if (tokc && item->text) {
            unsigned int t;
            for (t = 0; t < tokc && fstrstr(item->text, tokv[t]); ++t);
            if (t < tokc)
                continue;
        }

        if (f == 0 || item == bmMenuGetHighlightedItem(menu))
            *outHighlighted = f;
        filtered[f++] = item;
    }

    if (buffer)
        free(buffer);

    if (tokv)
        free(tokv);

    return _bmFilterShrinkList(&filtered, menu->items.count, (*outNmemb = f));
}

/**
 * Filter that mimics the vanilla dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenu(bmMenu *menu, unsigned int *outNmemb, unsigned int *outHighlighted)
{
    return _bmFilterDmenuFun(menu, strstr, outNmemb, outHighlighted);
}

/**
 * Filter that mimics the vanilla case-insensitive dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenuCaseInsensitive(bmMenu *menu, unsigned int *outNmemb, unsigned int *outHighlighted)
{
    return _bmFilterDmenuFun(menu, _bmStrupstr, outNmemb, outHighlighted);
}

/* vim: set ts=8 sw=4 tw=0 :*/
