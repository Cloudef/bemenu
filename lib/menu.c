#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

#include "vim.h"

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
    "#121212FF", // BM_COLOR_CURSOR_BG
    "#CACACAFF", // BM_COLOR_CURSOR_FG
    "#121212FF", // BM_COLOR_ITEM_BG
    "#CACACAFF", // BM_COLOR_ITEM_FG
    "#121212FF", // BM_COLOR_HIGHLIGHTED_BG
    "#D81860FF", // BM_COLOR_HIGHLIGHTED_FG
    "#D81860FF", // BM_COLOR_FEEDBACK_BG
    "#121212FF", // BM_COLOR_FEEDBACK_FG
    "#121212FF", // BM_COLOR_SELECTED_BG
    "#D81860FF", // BM_COLOR_SELECTED_FG
    "#121212FF", // BM_COLOR_ALTERNATE_BG
    "#CACACAFF", // BM_COLOR_ALTERNATE_FG
    "#121212FF", // BM_COLOR_SCROLLBAR_BG
    "#D81860FF", // BM_COLOR_SCROLLBAR_FG
    "#D81860FF", // BM_COLOR_BORDER
};

/**
 * Filter function map.
 */
static struct bm_item** (*filter_func[BM_FILTER_MODE_LAST])(struct bm_menu *menu, bool addition, uint32_t *out_nmemb) = {
    bm_filter_dmenu, /* BM_FILTER_DMENU */
    bm_filter_dmenu_case_insensitive /* BM_FILTER_DMENU_CASE_INSENSITIVE */
};

struct bm_menu*
bm_menu_new(const char *renderer)
{
    struct bm_menu *menu;
    if (!(menu = calloc(1, sizeof(struct bm_menu))))
        return NULL;

    menu->dirty = true;

    menu->key_binding = BM_KEY_BINDING_DEFAULT;
    menu->vim_mode = 'i';
    menu->vim_last_key = 0;

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

    if (strcmp(menu->filter ? menu->filter : "", filter ? filter : ""))
        menu->dirty = true;

    free(menu->filter);
    menu->filter_size = (filter ? strlen(filter) : 0);
    menu->filter = (menu->filter_size > 0 ? bm_strdup(filter) : NULL);
    menu->curses_cursor = (menu->filter ? bm_utf8_string_screen_width(menu->filter) : 0);
    menu->dirty |= (menu->cursor != menu->filter_size);
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
bm_menu_set_lines_mode(struct bm_menu *menu, enum bm_lines_mode mode)
{
    assert(menu);
    menu->lines_mode = mode;
}

enum bm_lines_mode
bm_menu_get_lines_mode(struct bm_menu *menu)
{
    assert(menu);
    return menu->lines_mode;
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

void
bm_menu_set_cursor_width(struct bm_menu *menu, uint32_t cursor_width)
{
    assert(menu);
    menu->cursor_width = cursor_width;
}

uint32_t
bm_menu_get_cursor_width(struct bm_menu *menu)
{
    assert(menu);
    return menu->cursor_width;
}

void
bm_menu_set_hpadding(struct bm_menu *menu, uint32_t hpadding)
{
    assert(menu);
    menu->hpadding = hpadding;
}

uint32_t
bm_menu_get_hpadding(struct bm_menu *menu)
{
    assert(menu);
    return menu->hpadding;
}

void
bm_menu_set_border_size(struct bm_menu* menu, double border_size)
{
    assert(menu);
    menu->border_size = border_size;
}

double
bm_menu_get_border_size(struct bm_menu* menu)
{
    assert(menu);
    return menu->border_size;
}

void
bm_menu_set_border_radius(struct bm_menu* menu, double border_radius)
{
    assert(menu);
    menu->border_radius = border_radius;
}

double
bm_menu_get_border_radius(struct bm_menu* menu)
{
    assert(menu);
    return menu->border_radius;
}

uint32_t
bm_menu_get_height(struct bm_menu *menu)
{
    assert(menu);

    uint32_t height = 0;

    if (menu->renderer->api.get_height) {
        height = menu->renderer->api.get_height(menu);
    }

    return height;
}

uint32_t
bm_menu_get_width(struct bm_menu *menu)
{
    assert(menu);

    uint32_t width = 0;

    if (menu->renderer->api.get_width) {
        width = menu->renderer->api.get_width(menu);
    }

    return width;
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
bm_menu_set_fixed_height(struct bm_menu *menu, bool mode)
{
    menu->fixed_height = mode;
}

bool
bm_menu_get_fixed_height(struct bm_menu *menu)
{
    return menu->fixed_height;
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
bm_menu_set_vim_esc_exits(struct bm_menu *menu, bool mode)
{
    menu->vim_esc_exits = mode;
}

bool
bm_menu_get_vim_esc_exits(struct bm_menu *menu)
{
    return menu->vim_esc_exits;
}

void
bm_menu_set_counter(struct bm_menu *menu, bool mode)
{
    menu->counter = mode;
}

bool
bm_menu_get_counter(struct bm_menu *menu)
{
    return menu->counter;
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
bm_menu_set_y_offset(struct bm_menu *menu, int32_t y_offset)
{
    assert(menu);

    if(menu->y_offset == y_offset)
        return;

    menu->y_offset = y_offset;

    if (menu->renderer->api.set_y_offset)
        menu->renderer->api.set_y_offset(menu, y_offset);
}

int32_t
bm_menu_get_y_offset(struct bm_menu *menu)
{
    assert(menu);
    return menu->y_offset;
}

void
bm_menu_set_width(struct bm_menu *menu, uint32_t margin, float factor)
{
    assert(menu);

    if(menu->hmargin_size == margin && menu->width_factor == factor)
        return;

    menu->hmargin_size = margin;
    menu->width_factor = factor;

    if(menu->renderer->api.set_width)
        menu->renderer->api.set_width(menu, margin, factor);
}

uint32_t
bm_menu_get_hmargin_size(struct bm_menu *menu)
{
    assert(menu);
    return menu->hmargin_size;
}

float
bm_menu_get_width_factor(struct bm_menu *menu)
{
    assert(menu);
    return menu->width_factor;
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
bm_menu_set_password(struct bm_menu *menu, enum bm_password_mode mode)
{
    assert(menu);

    if (menu->password == mode)
        return;

    menu->password = mode;
}

enum bm_password_mode
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
        index = (count > 0 ? count - 1 : 0);

    if (menu->index != index)
        menu->dirty = true;

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

    return (bm_menu_set_highlighted_index(menu, i));
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

void
bm_menu_set_key_binding(struct bm_menu *menu, enum bm_key_binding key_binding){
    menu->key_binding = key_binding;
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

bool
bm_menu_render(struct bm_menu *menu)
{
    assert(menu);

    if (menu->renderer->api.render)
        return menu->renderer->api.render(menu);

    return true;
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
    bm_menu_set_highlighted_index(menu, 0);

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

struct bm_pointer
bm_menu_poll_pointer(struct bm_menu *menu)
{
    assert(menu);

    struct bm_pointer pointer = {0};

    if (menu->renderer->api.poll_pointer)
        pointer = menu->renderer->api.poll_pointer(menu);

    return pointer;
}

struct bm_touch
bm_menu_poll_touch(struct bm_menu *menu)
{
    assert(menu);

    struct bm_touch touch = {0};

    if (menu->renderer->api.poll_touch)
        touch = menu->renderer->api.poll_touch(menu);

    return touch;
}

void
bm_menu_release_touch(struct bm_menu *menu)
{
    assert(menu);

    if (menu->renderer->api.release_touch)
        menu->renderer->api.release_touch(menu);
}

static void
menu_next(struct bm_menu *menu, uint32_t count, bool wrap)
{
    if (menu->index < count - 1) {
        bm_menu_set_highlighted_index(menu, menu->index + 1);
    } else if (wrap) {
        bm_menu_set_highlighted_index(menu, 0);
    }
}

static void
menu_prev(struct bm_menu *menu, uint32_t count, bool wrap)
{
    if (menu->index > 0) {
        bm_menu_set_highlighted_index(menu, menu->index - 1);
    } else if (wrap) {
        bm_menu_set_highlighted_index(menu, count - 1);
    }
}

static void
menu_point_select(struct bm_menu *menu, uint32_t posx, uint32_t posy, uint32_t displayed)
{
    (void) posx;

    if (displayed == 0) {
        return;
    }
    uint32_t line_height = bm_menu_get_height(menu) / displayed;

    if (line_height == 0) {
        return;
    }
    uint32_t selected_line = posy / line_height;

    assert(menu->lines != 0);
    uint16_t current_page_index = menu->index / menu->lines;

    if (0 == selected_line) { // Mouse over title bar
        bm_menu_set_highlighted_index(menu, current_page_index * menu->lines);
        return;
    }

    if (selected_line >= displayed) { // This might be useless
        return;
    }

    bm_menu_set_highlighted_index(menu, current_page_index * menu->lines + (selected_line - 1));
}

static void
menu_scroll_down(struct bm_menu *menu, uint16_t count)
{
    assert(menu->lines != 0);
    if (menu->index / menu->lines != count / menu->lines) { // not last page
        bm_menu_set_highlighted_index(menu, ((menu->index / menu->lines) + 1) * menu->lines);
    }
}

static void
menu_scroll_up(struct bm_menu *menu, uint16_t count)
{
    (void) count;
    assert(menu->lines != 0);
    if (menu->index / menu->lines) { // not first page
        bm_menu_set_highlighted_index(menu, ((menu->index / menu->lines) - 1) * menu->lines + menu->lines - 1);
    }
}

static void
menu_scroll_first(struct bm_menu *menu, uint16_t count)
{
    (void) count;
    bm_menu_set_highlighted_index(menu, 0);
}

static void
menu_scroll_last(struct bm_menu *menu, uint16_t count)
{
    bm_menu_set_highlighted_index(menu, count - 1);
}

void
bm_menu_add_event_feedback(struct bm_menu *menu, uint32_t event_feedback)
{
    menu->event_feedback |= event_feedback;
}

void
bm_menu_remove_event_feedback(struct bm_menu *menu, uint32_t event_feedback)
{
    menu->event_feedback &= ~event_feedback;
}

void
bm_menu_set_event_feedback(struct bm_menu *menu, uint32_t event_feedback)
{
    if (menu->event_feedback != event_feedback) {
        menu->dirty = true;
    }
    menu->event_feedback = event_feedback;
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

    if (key != BM_KEY_NONE)
        menu->dirty = true;

    if(menu->key_binding == BM_KEY_BINDING_VIM){
        enum bm_vim_code code = bm_vim_key_press(menu, key, unicode, count, displayed);

        switch(code){
            case BM_VIM_CONSUME:
                return BM_RUN_RESULT_RUNNING;
            case BM_VIM_EXIT:
                list_free_list(&menu->selection);
                return BM_RUN_RESULT_CANCEL;
            case BM_VIM_IGNORE:
                break;
        }
    }

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
            (menu->lines_mode == BM_LINES_UP ? menu_next(menu, count, menu->wrap) : menu_prev(menu, count, menu->wrap));
            break;

        case BM_KEY_DOWN:
            (menu->lines_mode == BM_LINES_UP ? menu_prev(menu, count, menu->wrap) : menu_next(menu, count, menu->wrap));
            break;

        case BM_KEY_PAGE_UP:
            bm_menu_set_highlighted_index(menu, (menu->index < displayed ? 0 : menu->index - (displayed - 1)));
            break;

        case BM_KEY_PAGE_DOWN:
            bm_menu_set_highlighted_index(menu, (menu->index + displayed >= count ? count - 1 : menu->index + (displayed - 1)));
            break;

        case BM_KEY_SHIFT_PAGE_UP:
            bm_menu_set_highlighted_index(menu, 0);
            break;

        case BM_KEY_SHIFT_PAGE_DOWN:
            bm_menu_set_highlighted_index(menu, count - 1);
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
                char *paster[] = {"wl-paste -t text/plain", "xclip -t text/plain -out"};
                for (size_t paster_i = 0; paster_i < sizeof paster / sizeof paster[0]; paster_i++) {
                    FILE *clipboard = popen(paster[paster_i], "r");
                    if (clipboard == NULL) {
                        fprintf(stderr, "popen() for '%s' failed: ", paster[paster_i]);
                        perror(NULL);
                        continue;
                    }

                    char *line = NULL;
                    size_t len = 0;
                    ssize_t nread;

                    char *pasted_buf = NULL;
                    size_t pasted_len = 0;

                    /* Buffer all output first. If the child doesn't exit
                     * with 0, we won't use this output and assume that it
                     * was garbage. */
                    while ((nread = getline(&line, &len, clipboard)) != -1) {
                        pasted_buf = realloc(pasted_buf, pasted_len + (size_t)nread);
                        if (pasted_buf == NULL) {
                            fprintf(stderr, "realloc() while reading from paster '%s' failed: ", paster[paster_i]);
                            perror(NULL);
                            continue;
                        }

                        memmove(pasted_buf + pasted_len, line, (size_t)nread);
                        pasted_len += (size_t)nread;
                    }

                    free(line);

                    int wstatus = pclose(clipboard);
                    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
                        for (size_t i = 0; i < pasted_len; i++) {
                            size_t width;
                            menu->cursor += bm_unicode_insert(&menu->filter, &menu->filter_size, menu->cursor, pasted_buf[i], &width);
                            menu->curses_cursor += width;
                        }
                        free(pasted_buf);
                        break;
                    } else if (wstatus == -1) {
                        fprintf(stderr, "Waiting for clipboard paste program '%s' failed: ", paster[paster_i]);
                        perror(NULL);
                    } else {
                        if (WIFEXITED(wstatus)) {
                            fprintf(stderr, "Clipboard paste program '%s' exited with %d, discarding\n", paster[paster_i], WEXITSTATUS(wstatus));
                        } else {
                            fprintf(stderr, "Clipboard paste program '%s' died abnormally, discarding\n", paster[paster_i]);
                        }
                    }

                    free(pasted_buf);
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

enum bm_run_result
bm_menu_run_with_pointer(struct bm_menu *menu, struct bm_pointer pointer)
{
    uint32_t count;
    bm_menu_get_filtered_items(menu, &count);

    uint32_t displayed = 0;
    if (menu->renderer->api.get_displayed_count)
        displayed = menu->renderer->api.get_displayed_count(menu);

    if (!displayed)
        displayed = count;

    if (!menu->lines) {
        if (pointer.axes[BM_POINTER_AXIS_VERTICAL].valid) {
            if (0 < pointer.axes[BM_POINTER_AXIS_VERTICAL].value) {
                menu_next(menu, count, menu->wrap);
            } else {
                menu_prev(menu, count, menu->wrap);
            }
        }
        if (pointer.event_mask & POINTER_EVENT_BUTTON && pointer.state == POINTER_STATE_RELEASED) {
            switch (pointer.button) {
                case BM_POINTER_KEY_PRIMARY:
                    {
                        struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                        if (highlighted && !bm_menu_item_is_selected(menu, highlighted))
                            list_add_item(&menu->selection, highlighted);
                    }
                    return BM_RUN_RESULT_SELECTED;
            }
        }
       return BM_RUN_RESULT_RUNNING;
    }

    if (pointer.axes[BM_POINTER_AXIS_VERTICAL].valid) {
        if (0 < pointer.axes[BM_POINTER_AXIS_VERTICAL].value) {
            menu_scroll_down(menu, count);
        } else {
            menu_scroll_up(menu, count);
        }
    }

    if (pointer.event_mask & POINTER_EVENT_MOTION) {
        menu_point_select(menu, pointer.pos_x, pointer.pos_y, displayed);
    }

    if (pointer.event_mask & POINTER_EVENT_BUTTON && pointer.state == POINTER_STATE_RELEASED) {
        switch (pointer.button) {
            case BM_POINTER_KEY_PRIMARY:
                {
                    struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                    if (highlighted && !bm_menu_item_is_selected(menu, highlighted))
                        list_add_item(&menu->selection, highlighted);
                }
                return BM_RUN_RESULT_SELECTED;
        }
    }

    return BM_RUN_RESULT_RUNNING;
}

enum bm_run_result
bm_menu_run_with_touch(struct bm_menu *menu, struct bm_touch touch)
{
    uint32_t count;
    bm_menu_get_filtered_items(menu, &count);

    uint32_t displayed = 0;
    if (menu->renderer->api.get_displayed_count)
        displayed = menu->renderer->api.get_displayed_count(menu);

    if (!displayed)
        displayed = count;

    if (!menu->lines) {
        // Not implemented yet
        return BM_RUN_RESULT_RUNNING;
    }

    uint16_t count_down = 0;
    for (size_t i = 0; i < 2; ++i) {
        struct bm_touch_point point = touch.points[i];
        if (point.event_mask & TOUCH_EVENT_DOWN) {
            count_down += 1;
        }
    }

    if (count_down == 2) {
        int16_t scroll_count = 0;
        int16_t scroll_directions[2];
        int16_t distance_trigger = displayed * bm_menu_get_line_height(menu) / 4;
        for (size_t i = 0; i < 2; ++i) {
            struct bm_touch_point point = touch.points[i];
            if (!(point.event_mask & TOUCH_EVENT_DOWN))
                continue;

            int32_t movement_y = point.pos_y - point.start_y;
            if (abs(movement_y) > distance_trigger) {
                scroll_directions[i] = movement_y / abs(movement_y);
                scroll_count++;
            }
        }

        int16_t scroll_direction_sum = scroll_directions[0] + scroll_directions[1];
        if (2 == abs(scroll_direction_sum)) {
            uint16_t current_page = menu->index / menu->lines;
            if (scroll_direction_sum < 0 && current_page != count / menu->lines) { // not already the first page
                menu_scroll_down(menu, count);
                bm_menu_release_touch(menu);
            } else if (scroll_direction_sum > 0 && current_page) { // not already the first page
                menu_scroll_up(menu, count);
                bm_menu_release_touch(menu);
            }
        }
    }

    if (count_down != 1)
        return BM_RUN_RESULT_RUNNING;

    for (size_t i = 0; i < 2; ++i) {
        struct bm_touch_point point = touch.points[i];
        if (!(point.event_mask & TOUCH_EVENT_DOWN))
            continue;

        menu_point_select(menu, point.pos_x, point.pos_y, displayed);
        if (point.pos_y < (int32_t) (bm_menu_get_height(menu) / displayed)) {
            if (point.pos_y < ((int32_t) (bm_menu_get_height(menu) / displayed)) - 50) {
                if (point.event_mask & TOUCH_EVENT_UP) {
                    menu_scroll_first(menu, count);
                    bm_menu_set_event_feedback(menu, 0);
                } else if (point.event_mask & TOUCH_EVENT_MOTION) {
                    bm_menu_set_event_feedback(menu, TOUCH_WILL_SCROLL_FIRST);
                }
            } else {
                if (point.event_mask & TOUCH_EVENT_UP) {
                    menu_scroll_up(menu, count);
                    bm_menu_set_event_feedback(menu, 0);
                } else if (point.event_mask & TOUCH_EVENT_MOTION) {
                    bm_menu_set_event_feedback(menu, TOUCH_WILL_SCROLL_UP);
                }
            }
        } else if ((uint32_t) point.pos_y > bm_menu_get_height(menu)) {
            if ((uint32_t) point.pos_y > bm_menu_get_height(menu) + 50) {
                if (point.event_mask & TOUCH_EVENT_UP) {
                    menu_scroll_last(menu, count);
                    bm_menu_set_event_feedback(menu, 0);
                } else if (point.event_mask & TOUCH_EVENT_MOTION) {
                    bm_menu_set_event_feedback(menu, TOUCH_WILL_SCROLL_LAST);
                }
            } else {
                if (point.event_mask & TOUCH_EVENT_UP) {
                    menu_scroll_down(menu, count);
                    bm_menu_set_event_feedback(menu, 0);
                } else if (point.event_mask & TOUCH_EVENT_MOTION) {
                    bm_menu_set_event_feedback(menu, TOUCH_WILL_SCROLL_DOWN);
                }
            }
        } else {
            if (point.pos_x > 0 && (uint32_t) point.pos_x < bm_menu_get_width(menu)) {
                bm_menu_set_event_feedback(menu, 0);

                if (point.event_mask & TOUCH_EVENT_UP) {
                    {
                        struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
                        if (highlighted && !bm_menu_item_is_selected(menu, highlighted))
                            list_add_item(&menu->selection, highlighted);
                    }
                    return BM_RUN_RESULT_SELECTED;
                }
            } else {
                if (!(point.event_mask & TOUCH_EVENT_UP)) {
                    bm_menu_set_event_feedback(menu, TOUCH_WILL_CANCEL);
                } else {
                    bm_menu_set_event_feedback(menu, 0);
                }
            }
        }
    }
    return BM_RUN_RESULT_RUNNING;
}

enum bm_run_result
bm_menu_run_with_events(struct bm_menu *menu, enum bm_key key,
    struct bm_pointer pointer, struct bm_touch touch, uint32_t unicode)
{
    enum bm_run_result key_result = bm_menu_run_with_key(menu, key, unicode);
    if (key_result != BM_RUN_RESULT_RUNNING) {
        return key_result;
    }

    enum bm_run_result pointer_result = bm_menu_run_with_pointer(menu, pointer);
    if (pointer_result != BM_RUN_RESULT_RUNNING) {
        return pointer_result;
    }

    return bm_menu_run_with_touch(menu, touch);
}

/* vim: set ts=8 sw=4 tw=0 :*/
