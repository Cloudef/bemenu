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
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param fstrstr Substring function used to match items.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenuFun(bmMenu *menu, char addition, char* (*fstrstr)(const char *a, const char *b), int (*fstrncmp)(const char *a, const char *b, size_t len), unsigned int *outNmemb, unsigned int *outHighlighted)
{
    assert(menu);
    assert(fstrstr);
    assert(fstrncmp);
    assert(outNmemb);
    assert(outHighlighted);
    *outNmemb = *outHighlighted = 0;

    unsigned int itemsCount;
    bmItem **items;

    if (addition) {
        items = bmMenuGetFilteredItems(menu, &itemsCount);
    } else {
        items = bmMenuGetItems(menu, &itemsCount);
    }

    char *buffer = NULL;
    bmItem **filtered = calloc(itemsCount, sizeof(bmItem*));
    if (!filtered)
        goto fail;

    char **tokv;
    unsigned int tokc;
    if (!(buffer = _bmFilterTokenize(menu, &tokv, &tokc)))
        goto fail;

    bmItem *highlighted = bmMenuGetHighlightedItem(menu);

    size_t len = (tokc ? strlen(tokv[0]) : 0);
    unsigned int i, f, e;
    for (e = f = i = 0; i < itemsCount; ++i) {
        bmItem *item = items[i];
        if (!item->text && tokc != 0)
            continue;

        if (tokc && item->text) {
            unsigned int t;
            for (t = 0; t < tokc && fstrstr(item->text, tokv[t]); ++t);
            if (t < tokc)
                continue;
        }

        if (item == highlighted)
            *outHighlighted = f;

        if (tokc && item->text && !fstrncmp(tokv[0], item->text, len + 1)) { /* exact matches */
            memmove(&filtered[1], filtered, f * sizeof(bmItem*));
            filtered[0] = item;
            e++; /* where do exact matches end */
        } else if (tokc && item->text && !fstrncmp(tokv[0], item->text, len)) { /* prefixes */
            memmove(&filtered[e + 1], &filtered[e], (f - e) * sizeof(bmItem*));
            filtered[e] = item;
            e++; /* where do exact matches end */
        } else {
            filtered[f] = item;
        }
        f++; /* where do all matches end */
    }

    if (buffer)
        free(buffer);
    if (tokv)
        free(tokv);

    return _bmFilterShrinkList(&filtered, menu->items.count, (*outNmemb = f));

fail:
    if (filtered)
        free(filtered);
    if (buffer)
        free(buffer);
    return NULL;
}

/**
 * Filter that mimics the vanilla dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenu(bmMenu *menu, char addition, unsigned int *outNmemb, unsigned int *outHighlighted)
{
    return _bmFilterDmenuFun(menu, addition, strstr, strncmp, outNmemb, outHighlighted);
}

/**
 * Filter that mimics the vanilla case-insensitive dmenu filtering.
 *
 * @param menu bmMenu instance to filter.
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param outNmemb unsigned int reference to filtered items outNmemb.
 * @param outHighlighted unsigned int reference to new outHighlighted item index.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** _bmFilterDmenuCaseInsensitive(bmMenu *menu, char addition, unsigned int *outNmemb, unsigned int *outHighlighted)
{
    return _bmFilterDmenuFun(menu, addition, _bmStrupstr, _bmStrnupcmp, outNmemb, outHighlighted);
}

/* vim: set ts=8 sw=4 tw=0 :*/
