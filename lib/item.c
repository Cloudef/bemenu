#include "internal.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct bm_item*
bm_item_new(const char *text)
{
    struct bm_item *item;
    if (!(item = calloc(1, sizeof(struct bm_item))))
        return NULL;

    bm_item_set_text(item, text);
    return item;
}

void
bm_item_free(struct bm_item *item)
{
    assert(item);
    free(item->text);
    free(item);
}

void
bm_item_set_userdata(struct bm_item *item, void *userdata)
{
    assert(item);
    item->userdata = userdata;
}

void*
bm_item_get_userdata(struct bm_item *item)
{
    assert(item);
    return item->userdata;
}

bool
bm_item_set_text(struct bm_item *item, const char *text)
{
    assert(item);

    char *copy = NULL;
    if (text && !(copy = bm_strdup(text)))
        return false;

    free(item->text);
    item->text = copy;
    return true;
}

const char*
bm_item_get_text(const struct bm_item *item)
{
    assert(item);
    return item->text;
}

/* vim: set ts=8 sw=4 tw=0 :*/
