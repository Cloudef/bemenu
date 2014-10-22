#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/**
 * Filter function map.
 */
static struct bm_item** (*filter_func[BM_FILTER_MODE_LAST])(struct bm_menu *menu, bool addition, uint32_t *out_nmemb) = {
    bm_filter_dmenu, /* BM_FILTER_DMENU */
    bm_filter_dmenu_case_insensitive /* BM_FILTER_DMENU_CASE_INSENSITIVE */
};

bool
bm_menu_item_is_selected(const struct bm_menu *menu, const struct bm_item *item)
{
    assert(menu);
    assert(item);

    uint32_t i, count;
    struct bm_item **items = bm_menu_get_selected_items(menu, &count);
    for (i = 0; i < count && items[i] != item; ++i);
    return (i < count);
}

struct bm_menu*
bm_menu_new(const char *renderer)
{
    struct bm_menu *menu;
    if (!(menu = calloc(1, sizeof(struct bm_menu))))
        return NULL;

    uint32_t count;
    const struct bm_renderer **renderers = bm_get_renderers(&count);

    bool status = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (renderer && strcmp(renderer, renderers[i]->name))
            continue;

        if (bm_renderer_activate((struct bm_renderer*)renderers[i], menu)) {
            status = true;
            menu->renderer = renderers[i];
            break;
        }
    }

    if (!status) {
        bm_menu_free(menu);
        return NULL;
    }

    return menu;
}

void
bm_menu_free(struct bm_menu *menu)
{
    assert(menu);

    if (menu->renderer && menu->renderer->api.destructor)
        menu->renderer->api.destructor(menu);

    free(menu->title);
    free(menu->filter);
    free(menu->old_filter);

    bm_menu_free_items(menu);
    free(menu);
}

void
bm_menu_free_items(struct bm_menu *menu)
{
    assert(menu);
    list_free_list(&menu->selection);
    list_free_list(&menu->filtered);
    list_free_items(&menu->items, (list_free_fun)bm_item_free);
}

void
bm_menu_set_userdata(struct bm_menu *menu, void *userdata)
{
    assert(menu);
    menu->userdata = userdata;
}

void*
bm_menu_get_userdata(struct bm_menu *menu)
{
    assert(menu);
    return menu->userdata;
}

void
bm_menu_set_filter(struct bm_menu *menu, const char *filter)
{
    assert(menu);

    free(menu->filter);
    menu->filter = (filter ? bm_strdup(filter) : NULL);
    menu->filter_size = (filter ? strlen(filter) : 0);
}

const char*
bm_menu_get_filter(struct bm_menu *menu)
{
    assert(menu);
    return menu->filter;
}

void
bm_menu_set_filter_mode(struct bm_menu *menu, enum bm_filter_mode mode)
{
    assert(menu);
    menu->filter_mode = (mode >= BM_FILTER_MODE_LAST ? BM_FILTER_MODE_DMENU : mode);
}

enum bm_filter_mode
bm_menu_get_filter_mode(const struct bm_menu *menu)
{
    assert(menu);
    return menu->filter_mode;
}

void
bm_menu_set_wrap(struct bm_menu *menu, bool wrap)
{
    assert(menu);
    menu->wrap = wrap;
}

bool
bm_menu_get_wrap(const struct bm_menu *menu)
{
    assert(menu);
    return menu->wrap;
}

bool
bm_menu_set_title(struct bm_menu *menu, const char *title)
{
    assert(menu);

    char *copy = NULL;
    if (title && !(copy = bm_strdup(title)))
        return false;

    free(menu->title);
    menu->title = copy;
    return true;
}

const char*
bm_menu_get_title(const struct bm_menu *menu)
{
    assert(menu);
    return menu->title;
}

bool
bm_menu_add_items_at(struct bm_menu *menu, struct bm_item *item, uint32_t index)
{
    assert(menu);
    return list_add_item_at(&menu->items, item, index);
}

bool
bm_menu_add_item(struct bm_menu *menu, struct bm_item *item)
{
    return list_add_item(&menu->items, item);
}

bool
bm_menu_remove_item_at(struct bm_menu *menu, uint32_t index)
{
    assert(menu);

    if (!menu->items.items || menu->items.count <= index)
        return 0;

    bool ret = list_remove_item_at(&menu->items, index);

    if (ret) {
        struct bm_item *item = ((struct bm_item**)menu->items.items)[index];
        list_remove_item(&menu->selection, item);
        list_remove_item(&menu->filtered, item);
    }

    return ret;
}

bool
bm_menu_remove_item(struct bm_menu *menu, struct bm_item *item)
{
    assert(menu);

    bool ret = list_remove_item(&menu->items, item);

    if (ret) {
        list_remove_item(&menu->selection, item);
        list_remove_item(&menu->filtered, item);
    }

    return ret;
}

bool
bm_menu_set_highlighted_index(struct bm_menu *menu, uint32_t index)
{
    assert(menu);

    uint32_t count;
    bm_menu_get_filtered_items(menu, &count);

    if (count <= index)
        return 0;

    return (menu->index = index);
}

bool
bm_menu_set_highlighted_item(struct bm_menu *menu, struct bm_item *item)
{
    assert(menu);

    uint32_t i, count;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    for (i = 0; i < count && items[i] != item; ++i);

    if (count <= i)
        return 0;

    return (menu->index = i);
}

struct bm_item*
bm_menu_get_highlighted_item(const struct bm_menu *menu)
{
    assert(menu);

    uint32_t count;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);

    if (!items || count <= menu->index)
        return NULL;

    return items[menu->index];
}

bool
bm_menu_set_selected_items(struct bm_menu *menu, struct bm_item **items, uint32_t nmemb)
{
    assert(menu);

    struct bm_item **new_items;
    if (!(new_items = calloc(sizeof(struct bm_item*), nmemb)))
        return 0;

    memcpy(new_items, items, sizeof(struct bm_item*) * nmemb);
    return list_set_items_no_copy(&menu->selection, new_items, nmemb);
}

struct bm_item**
bm_menu_get_selected_items(const struct bm_menu *menu, uint32_t *out_nmemb)
{
    assert(menu);
    return list_get_items(&menu->selection, out_nmemb);
}

bool
bm_menu_set_items(struct bm_menu *menu, const struct bm_item **items, uint32_t nmemb)
{
    assert(menu);

    bool ret = list_set_items(&menu->items, items, nmemb, (list_free_fun)bm_item_free);

    if (ret) {
        list_free_list(&menu->selection);
        list_free_list(&menu->filtered);
    }

    return ret;
}

struct bm_item**
bm_menu_get_items(const struct bm_menu *menu, uint32_t *out_nmemb)
{
    assert(menu);
    return list_get_items(&menu->items, out_nmemb);
}

struct bm_item**
bm_menu_get_filtered_items(const struct bm_menu *menu, uint32_t *out_nmemb)
{
    assert(menu);

    if (menu->filter && strlen(menu->filter))
        return list_get_items(&menu->filtered, out_nmemb);

    return list_get_items(&menu->items, out_nmemb);
}

void
bm_menu_render(const struct bm_menu *menu)
{
    assert(menu);

    if (menu->renderer->api.render)
        menu->renderer->api.render(menu);
}

void
bm_menu_filter(struct bm_menu *menu)
{
    assert(menu);

    char addition = 0;
    size_t len = (menu->filter ? strlen(menu->filter) : 0);

    if (!len || !menu->items.items || menu->items.count <= 0) {
        list_free_list(&menu->filtered);
        free(menu->old_filter);
        menu->old_filter = NULL;
        return;
    }

    if (menu->old_filter) {
        size_t oldLen = strlen(menu->old_filter);
        addition = (oldLen < len && !memcmp(menu->old_filter, menu->filter, oldLen));
    }

    if (menu->old_filter && addition && menu->filtered.count <= 0)
        return;

    if (menu->old_filter && !strcmp(menu->filter, menu->old_filter))
        return;

    uint32_t count;
    struct bm_item **filtered = filter_func[menu->filter_mode](menu, addition, &count);

    list_set_items_no_copy(&menu->filtered, filtered, count);
    menu->index = 0;

    free(menu->old_filter);
    menu->old_filter = bm_strdup(menu->filter);
}

enum bm_key
bm_menu_poll_key(struct bm_menu *menu, uint32_t *out_unicode)
{
    assert(menu && out_unicode);

    *out_unicode = 0;
    enum bm_key key = BM_KEY_NONE;

    if (menu->renderer->api.poll_key)
        key = menu->renderer->api.poll_key(menu, out_unicode);

    return key;
}

enum bm_run_result
bm_menu_run_with_key(struct bm_menu *menu, enum bm_key key, uint32_t unicode)
{
    assert(menu);

    uint32_t count;
    bm_menu_get_filtered_items(menu, &count);

    uint32_t displayed = 0;
    if (menu->renderer->api.get_displayed_count)
        displayed = menu->renderer->api.get_displayed_count(menu);

    if (!displayed)
        displayed = count;

    switch (key) {
        case BM_KEY_LEFT:
            if (menu->filter) {
                uint32_t oldCursor = menu->cursor;
                menu->cursor -= bm_utf8_rune_prev(menu->filter, menu->cursor);
                menu->curses_cursor -= bm_utf8_rune_width(menu->filter + menu->cursor, oldCursor - menu->cursor);
            }
            break;

        case BM_KEY_RIGHT:
            if (menu->filter) {
                uint32_t oldCursor = menu->cursor;
                menu->cursor += bm_utf8_rune_next(menu->filter, menu->cursor);
                menu->curses_cursor += bm_utf8_rune_width(menu->filter + oldCursor, menu->cursor - oldCursor);
            }
            break;

        case BM_KEY_HOME:
            menu->curses_cursor = menu->cursor = 0;
            break;

        case BM_KEY_END:
            menu->cursor = (menu->filter ? strlen(menu->filter) : 0);
            menu->curses_cursor = (menu->filter ? bm_utf8_string_screen_width(menu->filter) : 0);
            break;

        case BM_KEY_UP:
            if (menu->index > 0) {
                menu->index--;
            } else if (menu->wrap) {
                menu->index = count - 1;
            }
            break;

        case BM_KEY_DOWN:
            if (menu->index < count - 1) {
                menu->index++;
            } else if (menu->wrap) {
                menu->index = 0;
            }
            break;

        case BM_KEY_PAGE_UP:
            menu->index = (menu->index < displayed ? 0 : menu->index - (displayed - 1));
            break;

        case BM_KEY_PAGE_DOWN:
            menu->index = (menu->index + displayed >= count ? count - 1 : menu->index + (displayed - 1));
            break;

        case BM_KEY_SHIFT_PAGE_UP:
            menu->index = 0;
            break;

        case BM_KEY_SHIFT_PAGE_DOWN:
            menu->index = count - 1;
            break;

        case BM_KEY_BACKSPACE:
            if (menu->filter) {
                size_t width;
                menu->cursor -= bm_utf8_rune_remove(menu->filter, menu->cursor, &width);
                menu->curses_cursor -= width;
            }
            break;

        case BM_KEY_DELETE:
            if (menu->filter)
                bm_utf8_rune_remove(menu->filter, menu->cursor + 1, NULL);
            break;

        case BM_KEY_LINE_DELETE_LEFT:
            if (menu->filter) {
                while (menu->cursor > 0) {
                    size_t width;
                    menu->cursor -= bm_utf8_rune_remove(menu->filter, menu->cursor, &width);
                    menu->curses_cursor -= width;
                }
            }
            break;

        case BM_KEY_LINE_DELETE_RIGHT:
            if (menu->filter)
                menu->filter[menu->cursor] = 0;
            break;

        case BM_KEY_WORD_DELETE:
           if (menu->filter) {
                while (menu->cursor < strlen(menu->filter) && !isspace(menu->filter[menu->cursor])) {
                    uint32_t oldCursor = menu->cursor;
                    menu->cursor += bm_utf8_rune_next(menu->filter, menu->cursor);
                    menu->curses_cursor += bm_utf8_rune_width(menu->filter + oldCursor, menu->cursor - oldCursor);
                }
                while (menu->cursor > 0 && isspace(menu->filter[menu->cursor - 1])) {
                    uint32_t oldCursor = menu->cursor;
                    menu->cursor -= bm_utf8_rune_prev(menu->filter, menu->cursor);
                    menu->curses_cursor -= bm_utf8_rune_width(menu->filter + menu->cursor, oldCursor - menu->cursor);
                }
                while (menu->cursor > 0 && !isspace(menu->filter[menu->cursor - 1])) {
                    size_t width;
                    menu->cursor -= bm_utf8_rune_remove(menu->filter, menu->cursor, &width);
                    menu->curses_cursor -= width;
                }
            }
            break;

        case BM_KEY_UNICODE:
            {
                size_t width;
                menu->cursor += bm_unicode_insert(&menu->filter, &menu->filter_size, menu->cursor, unicode, &width);
                menu->curses_cursor += width;
            }
            break;

        case BM_KEY_TAB:
            {
                const char *text;
                struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                if (highlighted && (text = bm_item_get_text(highlighted))) {
                    bm_menu_set_filter(menu, text);
                    menu->cursor = (menu->filter ? strlen(menu->filter) : 0);
                    menu->curses_cursor = (menu->filter ? bm_utf8_string_screen_width(menu->filter) : 0);
                }
            }
            break;

        case BM_KEY_CONTROL_RETURN:
        case BM_KEY_RETURN:
            {
                struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                if (highlighted && !bm_menu_item_is_selected(menu, highlighted))
                    list_add_item(&menu->selection, highlighted);
            }
            break;

        case BM_KEY_SHIFT_RETURN:
        case BM_KEY_ESCAPE:
            list_free_list(&menu->selection);
            break;

        default: break;
    }

    bm_menu_filter(menu);

    switch (key) {
        case BM_KEY_SHIFT_RETURN:
        case BM_KEY_RETURN: return BM_RUN_RESULT_SELECTED;
        case BM_KEY_ESCAPE: return BM_RUN_RESULT_CANCEL;
        default: break;
    }

    return BM_RUN_RESULT_RUNNING;
}

/* vim: set ts=8 sw=4 tw=0 :*/
