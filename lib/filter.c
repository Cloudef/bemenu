#include "internal.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/**
 * Shrink struct bm_item** list pointer.
 *
 * Useful helper function for filter functions.
 *
 * @param in_out_list Pointer to pointer to list of bm_item pointers.
 * @param osize Current size of the list.
 * @param nsize New size the list will be shrinked to.
 * @return Pointer to list of bm_item pointers.
 */
static struct bm_item**
shrink_list(struct bm_item ***in_out_list, size_t osize, size_t nsize)
{
    assert(in_out_list);

    if (nsize == 0) {
        free(*in_out_list);
        return (*in_out_list = NULL);
    }

    if (nsize >= osize)
        return *in_out_list;

    void *tmp = malloc(sizeof(struct bm_item*) * nsize);
    if (!tmp)
        return *in_out_list;

    memcpy(tmp, *in_out_list, sizeof(struct bm_item*) * nsize);
    free(*in_out_list);
    return (*in_out_list = tmp);
}

/**
 * Text filter tokenizer helper.
 *
 * @param menu bm_menu instance which filter to tokenize.
 * @param out_tokv char pointer reference to list of tokens, this should be freed after use.
 * @param out_tokc uint32_t reference to number of tokens.
 * @return Pointer to buffer that contains tokenized string, this should be freed after use.
 */
static char*
tokenize(struct bm_menu *menu, char ***out_tokv, uint32_t *out_tokc)
{
    assert(menu && out_tokv && out_tokc);
    *out_tokv = NULL;
    *out_tokc = 0;

    char **tokv = NULL, *buffer = NULL;
    if (!(buffer = bm_strdup(menu->filter)))
        goto fail;

    char *s;
    for (s = buffer; *s && *s == ' '; ++s);

    char **tmp = NULL;
    size_t pos = 0, next;
    uint32_t tokc = 0, tokn = 0;
    for (; (pos = bm_strip_token(s, " ", &next)) > 0; tokv = tmp) {
        if (++tokc > tokn && !(tmp = realloc(tokv, ++tokn * sizeof(char*))))
            goto fail;

        tmp[tokc - 1] = s;
        s += next;
        for (; *s && *s == ' '; ++s);
    }

    *out_tokv = tmp;
    *out_tokc = tokc;
    return buffer;

fail:
    free(buffer);
    free(tokv);
    return NULL;
}

/**
 * Dmenu filterer that accepts substring function.
 *
 * @param menu bm_menu instance to filter.
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param fstrstr Substring function used to match items.
 * @param out_nmemb uint32_t reference to filtered items count.
 * @return Pointer to array of bm_item pointers.
 */
static struct bm_item**
filter_dmenu_fun(struct bm_menu *menu, char addition, char* (*fstrstr)(const char *a, const char *b), int (*fstrncmp)(const char *a, const char *b, size_t len), uint32_t *out_nmemb)
{
    assert(menu && fstrstr && fstrncmp && out_nmemb);
    *out_nmemb = 0;

    uint32_t count;
    struct bm_item **items;

    if (addition) {
        items = bm_menu_get_filtered_items(menu, &count);
    } else {
        items = bm_menu_get_items(menu, &count);
    }

    char *buffer = NULL;
    struct bm_item **filtered;
    if (!(filtered = calloc(count, sizeof(struct bm_item*))))
        goto fail;

    char **tokv;
    uint32_t tokc;
    if (!(buffer = tokenize(menu, &tokv, &tokc)))
        goto fail;

    const char *filter = menu->filter ? menu->filter : "";
    size_t len = (tokc ? strlen(tokv[0]) : 0);
    uint32_t i, f, e;
    for (e = f = i = 0; i < count; ++i) {
        struct bm_item *item = items[i];
        if (!item->text && tokc != 0)
            continue;

        if (tokc && item->text) {
            uint32_t t;
            for (t = 0; t < tokc && fstrstr(item->text, tokv[t]); ++t);
            if (t < tokc)
                continue;
        }

        if (tokc && item->text && strlen(filter) == strlen(item->text) && !fstrncmp(filter, item->text, strlen(filter))) { /* exact matches */
            memmove(&filtered[1], filtered, f * sizeof(struct bm_item*));
            filtered[0] = item;
            e++; /* where do exact matches end */
        } else if (tokc && item->text && !fstrncmp(tokv[0], item->text, len)) { /* prefixes */
            memmove(&filtered[e + 1], &filtered[e], (f - e) * sizeof(struct bm_item*));
            filtered[e] = item;
            e++; /* where do prefix matches end */
        } else {
            filtered[f] = item;
        }
        f++; /* where do all matches end */
    }

    free(buffer);
    free(tokv);
    return shrink_list(&filtered, menu->items.count, (*out_nmemb = f));

fail:
    free(filtered);
    free(buffer);
    return NULL;
}

/**
 * Filter that mimics the vanilla dmenu filtering.
 *
 * @param menu bm_menu instance to filter.
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param outNmemb uint32_t reference to filtered items count.
 * @return Pointer to array of bm_item pointers.
 */
struct bm_item**
bm_filter_dmenu(struct bm_menu *menu, bool addition, uint32_t *out_nmemb)
{
    return filter_dmenu_fun(menu, addition, strstr, strncmp, out_nmemb);
}

/**
 * Filter that mimics the vanilla case-insensitive dmenu filtering.
 *
 * @param menu bm_menu instance to filter.
 * @param addition This will be 1, if filter is same as previous filter with something appended.
 * @param outNmemb uint32_t reference to filtered items count.
 * @return Pointer to array of bm_item pointers.
 */
struct bm_item**
bm_filter_dmenu_case_insensitive(struct bm_menu *menu, bool addition, uint32_t *out_nmemb)
{
    return filter_dmenu_fun(menu, addition, bm_strupstr, bm_strnupcmp, out_nmemb);
}

/* vim: set ts=8 sw=4 tw=0 :*/
