/**
 * @file bemenu.c
 */

#include "internal.h"
#include <stdlib.h>
#include <assert.h>

/**
 * Create new bmMenu instance.
 *
 * @param drawMode Render method to be used for this menu instance.
 * @return bmMenu for new menu instance, NULL if creation failed.
 */
bmMenu* bmMenuNew(bmDrawMode drawMode)
{
    bmMenu *menu = calloc(1, sizeof(bmMenu));

    menu->drawMode = drawMode;

    if (!menu)
        return NULL;

    switch (menu->drawMode) {
        default:break;
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
    assert(menu != NULL);
    free(menu);
}

/**
 * Create new bmMenu instance.
 *
 * @param drawMode Render method to be used for this menu instance.
 * @return bmMenu for new menu instance, NULL if creation failed.
 */
void bmMenuRender(bmMenu *menu)
{
    assert(menu != NULL);

    if (menu->renderApi.render)
        menu->renderApi.render(menu->items, menu->itemsCount);
}

/* vim: set ts=8 sw=4 tw=0 :*/
