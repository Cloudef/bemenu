/**
 * @file internal.h
 */

#include "bemenu.h"

/**
 * Internal bmItem struct that is not exposed to public.
 * Represents a single item in menu.
 */
struct _bmItem {
    char *text;
};

/**
 * Internal bmRenderApi struct.
 * Renderers should be able to fill this one as they see fit.
 */
struct _bmRenderApi {
    void (*render)(struct _bmItem **items, unsigned int nmemb);
    void (*free)(void);
};

/**
 * Internal bmMenu struct that is not exposed to public.
 */
struct _bmMenu {
    bmDrawMode drawMode;
    struct _bmRenderApi renderApi;
    struct _bmItem **items;
    unsigned int itemsCount;
};

/* vim: set ts=8 sw=4 tw=0 :*/
