#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

/**
 * Default font.
 */
static const char *default_font = "monospace 10";

/**
 * Default hexadecimal colors.
 */
static const char *default_colors[BM_COLOR_LAST] = {
    "#121212FF", // BM_COLOR_TITLE_BG
    "#D81860FF", // BM_COLOR_TITLE_FG
    "#121212FF", // BM_COLOR_FILTER_BG
    "#CACACAFF", // BM_COLOR_FILTER_FG
    "#121212FF", // BM_COLOR_ITEM_BG
    "#CACACAFF", // BM_COLOR_ITEM_FG
    "#121212FF", // BM_COLOR_HIGHLIGHTED_BG
    "#D81860FF", // BM_COLOR_HIGHLIGHTED_FG
    "#121212FF", // BM_COLOR_SELECTED_BG
    "#D81860FF", // BM_COLOR_SELECTED_FG
    "#121212FF", // BM_COLOR_SCROLLBAR_BG
    "#D81860FF", // BM_COLOR_SCROLLBAR_FG
};

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

    const char *name = secure_getenv("BEMENU_BACKEND");
    name = (name && strlen(name) > 0 ? name : NULL);

    for (uint32_t i = 0; i < count; ++i) {
        if (!name && !renderer && renderers[i]->api.priorty != BM_PRIO_GUI)
            continue;

        if ((renderer && strcmp(renderer, renderers[i]->name)) || (name && strcmp(name, renderers[i]->name)))
            continue;

        if (renderers[i]->api.priorty == BM_PRIO_TERMINAL) {
            /**
             * Some sanity checks that we are in terminal.
             * These however are not reliable, thus we don't auto-detect terminal based renderers.
             * These will be only used when explicitly requested.
             *
             * Launching terminal based menu instance at background is not a good idea.
             */
            const char *term = getenv("TERM");
            if (!term || !strlen(term) || getppid() == 1)
                continue;
        }

        if (bm_renderer_activate((struct bm_renderer*)renderers[i], menu))
            break;
    }

    if (!menu->renderer)
        goto fail;

    if (!bm_menu_set_font(menu, NULL))
        goto fail;

    for (uint32_t i = 0; i < BM_COLOR_LAST; ++i) {
        if (!bm_menu_set_color(menu, i, NULL))
            goto fail;
    }

    if (!(menu->filter_item = bm_item_new(NULL)))
        goto fail;

    return menu;

fail:
    bm_menu_free(menu);
    return NULL;
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
    free(menu->font.name);

    free(menu->monitor_name);

    for (uint32_t i = 0; i < BM_COLOR_LAST; ++i)
        free(menu->colors[i].hex);

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

    if (menu->filter_item)
        free(menu->filter_item);
}

const struct bm_renderer*
bm_menu_get_renderer(struct bm_menu *menu)
{
    assert(menu);
    return menu->renderer;
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
bm_menu_set_prefix(struct bm_menu *menu, const char *prefix)
{
    assert(menu);
    free(menu->prefix);
    menu->prefix = (prefix && strlen(prefix) > 0 ? bm_strdup(prefix) : NULL);
}

const char*
bm_menu_get_prefix(struct bm_menu *menu)
{
    assert(menu);
    return menu->prefix;
}

void
bm_menu_set_filter(struct bm_menu *menu, const char *filter)
{
    assert(menu);

    free(menu->filter);
    menu->filter_size = (filter ? strlen(filter) : 0);
    menu->filter = (menu->filter_size > 0 ? bm_strdup(filter) : NULL);
    menu->curses_cursor = (menu->filter ? bm_utf8_string_screen_width(menu->filter) : 0);
    menu->cursor = menu->filter_size;
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
bm_menu_set_lines(struct bm_menu *menu, uint32_t lines)
{
    assert(menu);
    menu->lines = lines;
}

uint32_t
bm_menu_get_lines(struct bm_menu *menu)
{
    assert(menu);
    return menu->lines;
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
bm_menu_set_font(struct bm_menu *menu, const char *font)
{
    assert(menu);

    const char *nfont = (font ? font : default_font);

    char *copy = NULL;
    if (!(copy = bm_strdup(nfont)))
        return false;

    free(menu->font.name);
    menu->font.name = copy;
    return true;
}

const char*
bm_menu_get_font(const struct bm_menu *menu)
{
    assert(menu);
    return menu->font.name;
}

void
bm_menu_set_line_height(struct bm_menu *menu, uint32_t line_height)
{
    assert(menu);
    menu->line_height = line_height;
}

uint32_t
bm_menu_get_line_height(struct bm_menu *menu)
{
    assert(menu);
    return menu->line_height;
}

void
bm_menu_set_cursor_height(struct bm_menu *menu, uint32_t cursor_height)
{
    assert(menu);
    menu->cursor_height = cursor_height;
}

uint32_t
bm_menu_get_cursor_height(struct bm_menu *menu)
{
    assert(menu);
    return menu->cursor_height;
}

bool
bm_menu_set_color(struct bm_menu *menu, enum bm_color color, const char *hex)
{
    assert(menu);

    const char *nhex = (hex ? hex : default_colors[color]);

    unsigned int r, g, b, a = 255;
    int matched = sscanf(nhex, "#%2x%2x%2x%2x", &r, &b, &g, &a);
    if (matched != 3 && matched != 4)
        return false;

    char *copy = NULL;
    if (!(copy = bm_strdup(nhex)))
        return false;

    free(menu->colors[color].hex);
    menu->colors[color].hex = copy;
    menu->colors[color].r = r;
    menu->colors[color].g = g;
    menu->colors[color].b = b;
    menu->colors[color].a = a;
    return true;
}

const
char* bm_menu_get_color(const struct bm_menu *menu, enum bm_color color)
{
    assert(menu);
    return menu->colors[color].hex;
}

void
bm_menu_set_scrollbar(struct bm_menu *menu, enum bm_scrollbar_mode mode)
{
    menu->scrollbar = (mode == BM_SCROLLBAR_LAST ? BM_SCROLLBAR_NONE : mode);
}

enum bm_scrollbar_mode
bm_menu_get_scrollbar(struct bm_menu *menu)
{
    return menu->scrollbar;
}

void
bm_menu_set_align(struct bm_menu *menu, enum bm_align align)
{
    assert(menu);

    if(menu->align == align)
        return;

    menu->align = align;

    if (menu->renderer->api.set_align)
        menu->renderer->api.set_align(menu, align);
}

enum bm_align
bm_menu_get_align(struct bm_menu *menu)
{
    assert(menu);
    return menu->align;
}

void
bm_menu_set_hmargin_size(struct bm_menu *menu, uint32_t margin)
{
    assert(menu);

    if(menu->hmargin_size == margin)
        return;

    menu->hmargin_size = margin;

    if(menu->renderer->api.set_hmargin_size)
        menu->renderer->api.set_hmargin_size(menu, margin);
}

uint32_t
bm_menu_get_hmargin_size(struct bm_menu *menu)
{
    assert(menu);
    return menu->hmargin_size;
}

void
bm_menu_set_monitor(struct bm_menu *menu, int32_t monitor)
{
    assert(menu);

    if (menu->monitor == monitor)
        return;

    menu->monitor = monitor;

    if (menu->renderer->api.set_monitor)
        menu->renderer->api.set_monitor(menu, monitor);
}

void
bm_menu_set_monitor_name(struct bm_menu *menu, char *monitor_name)
{
    assert(menu);

    if (!monitor_name)
        return;

    if (menu->monitor_name && !strcmp(menu->monitor_name, monitor_name))
        return;

    menu->monitor_name = bm_strdup(monitor_name);

    if (menu->renderer->api.set_monitor_name)
        menu->renderer->api.set_monitor_name(menu, monitor_name);
}

uint32_t
bm_menu_get_monitor(struct bm_menu *menu)
{
    assert(menu);
    return menu->monitor;
}

void
bm_menu_grab_keyboard(struct bm_menu *menu, bool grab)
{
    assert(menu);

    if (menu->grabbed == grab)
        return;

    menu->grabbed = grab;

    if (menu->renderer->api.grab_keyboard)
        menu->renderer->api.grab_keyboard(menu, grab);
}

bool
bm_menu_is_keyboard_grabbed(struct bm_menu *menu)
{
    return menu->grabbed;
}

void
bm_menu_set_panel_overlap(struct bm_menu *menu, bool overlap)
{
    assert(menu);

    if (menu->overlap == overlap)
        return;

    menu->overlap = overlap;

    if (menu->renderer->api.set_overlap)
        menu->renderer->api.set_overlap(menu, overlap);
}

void
bm_menu_set_spacing(struct bm_menu *menu, bool spacing)
{
    assert(menu);

    if (menu->spacing == spacing)
        return;

    menu->spacing = spacing;
}

void
bm_menu_set_password(struct bm_menu *menu, bool password)
{
    assert(menu);

    if (menu->password == password)
        return;

    menu->password = password;
}

bool
bm_menu_get_password(struct bm_menu *menu)
{
    assert(menu);
    return menu->password;
}

bool
bm_menu_add_item_at(struct bm_menu *menu, struct bm_item *item, uint32_t index)
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

    struct bm_item *item = ((struct bm_item**)menu->items.items)[index];
    bool ret = list_remove_item_at(&menu->items, index);

    if (ret) {
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

static void
menu_next(struct bm_menu *menu, uint32_t count, bool wrap)
{
    if (menu->index < count - 1) {
        menu->index++;
    } else if (wrap) {
        menu->index = 0;
    }
}

static void
menu_prev(struct bm_menu *menu, uint32_t count, bool wrap)
{
    if (menu->index > 0) {
        menu->index--;
    } else if (wrap) {
        menu->index = count - 1;
    }
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
                const uint32_t old_cursor = menu->cursor;
                menu->cursor -= bm_utf8_rune_prev(menu->filter, menu->cursor);
                menu->curses_cursor -= bm_utf8_rune_width(menu->filter + menu->cursor, old_cursor - menu->cursor);
            }
            break;

        case BM_KEY_RIGHT:
            if (menu->filter) {
                const uint32_t old_cursor = menu->cursor;
                menu->cursor += bm_utf8_rune_next(menu->filter, menu->cursor);
                menu->curses_cursor += bm_utf8_rune_width(menu->filter + old_cursor, menu->cursor - old_cursor);
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
            menu_prev(menu, count, menu->wrap);
            break;

        case BM_KEY_DOWN:
            menu_next(menu, count, menu->wrap);
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
            if (menu->filter) {
                size_t width = bm_utf8_rune_next(menu->filter, menu->cursor);
                if (width)
                    bm_utf8_rune_remove(menu->filter, menu->cursor + width, NULL);
            }
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

        case BM_KEY_PASTE:
            {
                FILE *clipboard;
                clipboard = popen("wl-paste -t text/plain", "r");
                if (clipboard == NULL) {
                    clipboard = popen("xclip -t text/plain -out", "r");
                }
                if (clipboard == NULL) {
                    break;
                }
                int c;
                while ((c = fgetc(clipboard)) != EOF) {
                    size_t width;
                    menu->cursor += bm_unicode_insert(&menu->filter, &menu->filter_size, menu->cursor, c, &width);
                    menu->curses_cursor += width;
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
                menu_next(menu, count, true);
            }
            break;

        case BM_KEY_SHIFT_TAB:
            {
                const char *text;
                struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                if (highlighted && (text = bm_item_get_text(highlighted)))
                    bm_menu_set_filter(menu, text);
            }
            break;

        case BM_KEY_CONTROL_RETURN:
        case BM_KEY_RETURN:
        case BM_KEY_CUSTOM_1:
        case BM_KEY_CUSTOM_2:
        case BM_KEY_CUSTOM_3:
        case BM_KEY_CUSTOM_4:
        case BM_KEY_CUSTOM_5:
        case BM_KEY_CUSTOM_6:
        case BM_KEY_CUSTOM_7:
        case BM_KEY_CUSTOM_8:
        case BM_KEY_CUSTOM_9:
        case BM_KEY_CUSTOM_10:
            {
                struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                if (highlighted && !bm_menu_item_is_selected(menu, highlighted))
                    list_add_item(&menu->selection, highlighted);
            }
            break;

        case BM_KEY_SHIFT_RETURN: /* this will return the filter as selected item below! */
        case BM_KEY_ESCAPE: /* this will cancel however */
            list_free_list(&menu->selection);
            break;

        default: break;
    }

    bm_menu_filter(menu);

    switch (key) {
        case BM_KEY_CUSTOM_1:
        case BM_KEY_CUSTOM_2:
        case BM_KEY_CUSTOM_3:
        case BM_KEY_CUSTOM_4:
        case BM_KEY_CUSTOM_5:
        case BM_KEY_CUSTOM_6:
        case BM_KEY_CUSTOM_7:
        case BM_KEY_CUSTOM_8:
        case BM_KEY_CUSTOM_9:
        case BM_KEY_CUSTOM_10:
        case BM_KEY_SHIFT_RETURN:
        case BM_KEY_RETURN:
            if (!bm_menu_get_selected_items(menu, NULL)) {
                bm_item_set_text(menu->filter_item, menu->filter);
                list_add_item(&menu->selection, menu->filter_item);
            }
            switch (key) {
                case BM_KEY_CUSTOM_1: return BM_RUN_RESULT_CUSTOM_1;
                case BM_KEY_CUSTOM_2: return BM_RUN_RESULT_CUSTOM_2;
                case BM_KEY_CUSTOM_3: return BM_RUN_RESULT_CUSTOM_3;
                case BM_KEY_CUSTOM_4: return BM_RUN_RESULT_CUSTOM_4;
                case BM_KEY_CUSTOM_5: return BM_RUN_RESULT_CUSTOM_5;
                case BM_KEY_CUSTOM_6: return BM_RUN_RESULT_CUSTOM_6;
                case BM_KEY_CUSTOM_7: return BM_RUN_RESULT_CUSTOM_7;
                case BM_KEY_CUSTOM_8: return BM_RUN_RESULT_CUSTOM_8;
                case BM_KEY_CUSTOM_9: return BM_RUN_RESULT_CUSTOM_9;
                case BM_KEY_CUSTOM_10: return BM_RUN_RESULT_CUSTOM_10;
                default: break;
            }
            return BM_RUN_RESULT_SELECTED;

        case BM_KEY_ESCAPE: return BM_RUN_RESULT_CANCEL;
        default: break;
    }

    return BM_RUN_RESULT_RUNNING;
}

/* vim: set ts=8 sw=4 tw=0 :*/
