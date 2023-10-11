#ifndef _BEMENU_INTERNAL_H_
#define _BEMENU_INTERNAL_H_

#include "bemenu.h"

#if __GNUC__
#  define BM_LOG_ATTR(x, y) __attribute__((format(printf, x, y)))
#else
#  define BM_LOG_ATTR(x, y)
#endif

#ifndef __GLIBC__
#   define secure_getenv getenv
#else
extern char *secure_getenv(const char *name);
#endif

#include <stddef.h> /* for size_t */
#include <stdarg.h>

//minimum allowed window width when setting margin
#define WINDOW_MIN_WIDTH 80

/**
 * Destructor function pointer for some list calls.
 */
typedef void (*list_free_fun)(void*);

/**
 * List type
 */
struct list {
    /**
     * Items in the list.
     */
    void **items;

    /**
     * Number of items.
     */
    uint32_t count;

    /**
     * Number of allocated items.
     */
    uint32_t allocated;
};

/**
 * Internal render api struct.
 * Renderers should be able to fill this one as they see fit.
 */
struct render_api {
    /**
     * Create underlying renderer.
     */
    bool (*constructor)(struct bm_menu *menu);

    /**
     * Release underlying renderer.
     */
    void (*destructor)(struct bm_menu *menu);

    /**
     * Get count of displayed items by the underlying renderer.
     */
    uint32_t (*get_displayed_count)(const struct bm_menu *menu);

    /**
     * Get height by the underlying renderer;
     */
    uint32_t (*get_height)(const struct bm_menu *menu);

    /**
     * Get width by the underlying renderer;
     */
    uint32_t (*get_width)(const struct bm_menu *menu);

    /**
     * If the underlying renderer is a UI toolkit. (curses, etc...)
     * There might be possibility to get user input, and this should be thus implemented.
     */
    enum bm_key (*poll_key)(const struct bm_menu *menu, uint32_t *unicode);

    /**
     * If the underlying renderer is a UI toolkit. (curses, etc...)
     * There might be possibility to get user pointer, and this should be thus implemented.
     */
    struct bm_pointer (*poll_pointer)(const struct bm_menu *menu);

    /**
     * If the underlying renderer is a UI toolkit. (curses, etc...)
     * There might be possibility to get user touch, and this should be thus implemented.
     */
    struct bm_touch (*poll_touch)(const struct bm_menu *menu);

    /**
     * Enforce a release of the touches
     * There might be possibility to get user touch, and this should be thus implemented.
     */
    void (*release_touch)(const struct bm_menu *menu);

    /**
     * Tells underlying renderer to draw the menu.
     */
    bool (*render)(struct bm_menu *menu);

    /**
     * Set vertical alignment of the bar.
     */
    void (*set_align)(const struct bm_menu *menu, enum bm_align align);

    /**
     * Set vertical/y offset of the window.
     */
    void (*set_y_offset)(const struct bm_menu *menu, int32_t y_offset);

    /**
     * Set horizontal margin and relative width factor.
     */
    void (*set_width)(const struct bm_menu *menu, uint32_t margin, float factor);

    /**
     * Set monitor indeax where menu will appear
     */
    void (*set_monitor)(const struct bm_menu *menu, int32_t monitor);

    /**
     * Set monitor name where menu will appear
     */
    void (*set_monitor_name)(const struct bm_menu *menu, char *monitor_name);

    /**
     * Grab/Ungrab keyboard
     */
    void (*grab_keyboard)(const struct bm_menu *menu, bool grab);

    /**
     * Control overlap with panels
     */
    void (*set_overlap)(const struct bm_menu *menu, bool overlap);

    /**
     * Version of the plugin.
     * Should match BM_PLUGIN_VERSION or failure.
     */
    const char *version;

    /**
     * Priorty of the plugin.
     * Terminal renderers should be first, then graphicals.
     */
    enum bm_priorty priorty;
};

/**
 * Internal bm_renderer struct.
 */
struct bm_renderer {
    /**
     * Name of the renderer.
     */
    char *name;

    /**
     * File path of the renderer.
     */
    char *file;

    /**
     * Open handle to the plugin file of this renderer.
     */
    void *handle;

    /**
     * Data used by the renderer internally.
     * Nobody else should touch this.
     */
    void *internal;

    /**
     * API
     */
    struct render_api api;
};

/**
 * Internal bm_item struct that is not exposed to public.
 * Represents a single item in menu.
 */
struct bm_item {
    /**
     * Userdata pointer.
     * This pointer will be passed around with the item untouched.
     */
    void *userdata;

    /**
     * Primary text shown on item as null terminated C "string".
     * Matching will be done against this text as well.
     */
    char *text;
};

/**
 * Internal bm_hex_color struct that is not exposed to public.
 * Represent a color for element.
 */
struct bm_hex_color {
    /**
     * Provided hex for the color.
     */
    char *hex;

    /**
     * RGB values.
     */
    uint8_t r, g, b, a;
};

/**
 * Internal bm_font struct that is not exposed to public.
 * Represent a font for text.
 */
struct bm_font {
    /**
     * Name of the font.
     */
    char *name;
};

/**
 * Internal bm_menu struct that is not exposed to public.
 */
struct bm_menu {
    /**
     * Userdata pointer.
     * This pointer will be passed around with the menu untouched.
     */
    void *userdata;

    /**
     * Underlying renderer access.
     */
    struct bm_renderer *renderer;

    /**
     * Items contained in menu instance.
     */
    struct list items;

    /**
     * Filtered/displayed items contained in menu instance.
     */
    struct list filtered;

    /**
     * Selected items.
     */
    struct list selection;

    /**
     * Menu instance title.
     */
    char *title;

    /**
     * Font.
     */
    struct bm_font font;

    /**
     * Line height.
     */
    uint32_t line_height;

    /**
     * Cursor height.
     */
    uint32_t cursor_height;

    /**
     * Cursor width.
     */
    uint32_t cursor_width;

    /**
     * Horizontal Padding.
     */
    uint32_t hpadding;

    /**
     * Colors.
     */
    struct bm_hex_color colors[BM_COLOR_LAST];

    /**
     * Prefix shown for highlighted item.
     * Vertical mode only.
     */
    char *prefix;

    /**
     * Text used to filter matches.
     */
    char *filter;

    /**
     * Used as optimization.
     */
    char *old_filter;

    /**
     * Used when selecting the filter text (ex. SHIFT_RETURN)
     */
    struct bm_item *filter_item;

    /**
     * Size of filter buffer
     */
    size_t filter_size;

    /**
     * Current byte offset on filter text.
     */
    uint32_t cursor;

    /**
     * Current column/cursor position on filter text.
     */
    uint32_t curses_cursor;

    /**
     * Current filtered/highlighted item index in menu instance.
     * This index is valid for the list returned by bmMenuGetFilteredItems.
     */
    uint32_t index;

    /**
     * Max number of vertical lines to be shown.
     * Some renderers such as ncurses may ignore this when it does not make sense.
     */
    uint32_t lines;

    /**
     * Current vertical line mode.
     */
    enum bm_lines_mode lines_mode;

    /**
     * Current monitor.
     */
    int32_t monitor;

    /**
     * Current monitor name. Wayland only.
     */
    char *monitor_name;

    /**
     * Current filtering method in menu instance.
     */
    enum bm_filter_mode filter_mode;

    /**
     * Current fixed height mode.
     */
    bool fixed_height;

    /**
     * Current Scrollbar display mode.
     */
    enum bm_scrollbar_mode scrollbar;

    /**
     * Current vim escape exit mode.
     */
    bool vim_esc_exits; 

    /**
     * Current counter display mode.
     */
    bool counter;

    /**
     * Should selection be wrapped?
     */
    bool wrap;

    /**
     * Vertical alignment.
     */
    enum bm_align align;

    /**
     * Vertical/y offset.
     */
    int32_t y_offset;

    /**
     * Horizontal margin.
     */
    uint32_t hmargin_size;

    /**
     * Relative width factor.
     */
    float width_factor;

    /**
     * Border size
     */
    double border_size;

    /**
     * Border radius
     */
    double border_radius;

    /**
     * Is menu grabbed?
     */
    bool grabbed;

    /**
     * Should the menu overlap panels
     */
    bool overlap;

    /**
     * Should the input be displayed/hidden/replaced
     */
    enum bm_password_mode password;

    /**
     * Should the entry should follow the title spacing
     */
    bool spacing;

    /**
     * Mask representing a feedback to bring to user
     */
    uint32_t event_feedback;

    /**
     * Is the menu needing a redraw ?
     */
    bool dirty;

    /**
     * Key binding that should be used.
     */
    enum bm_key_binding key_binding;

    /**
     * Vim binding specific variables.
     */
    char vim_mode;
    uint32_t vim_last_key;
};

/* library.c */
bool bm_renderer_activate(struct bm_renderer *renderer, struct bm_menu *menu);

/* filter.c */
struct bm_item** bm_filter_dmenu(struct bm_menu *menu, bool addition, uint32_t *out_nmemb);
struct bm_item** bm_filter_dmenu_case_insensitive(struct bm_menu *menu, bool addition, uint32_t *out_nmemb);

/* list.c */
void list_free_list(struct list *list);
void list_free_items(struct list *list, list_free_fun destructor);
void* list_get_items(const struct list *list, uint32_t *out_nmemb);
bool list_set_items_no_copy(struct list *list, void *items, uint32_t nmemb);
bool list_set_items(struct list *list, const void *items, uint32_t nmemb, list_free_fun destructor);
bool list_grow(struct list *list, uint32_t step);
bool list_add_item_at(struct list *list, void *item, uint32_t index);
bool list_add_item(struct list *list, void *item);
bool list_remove_item_at(struct list *list, uint32_t index);
bool list_remove_item(struct list *list, const void *item);
void list_sort(struct list *list, int (*compar)(const void *a, const void *b));

/* util.c
 * Functions here may be used in renderers also. They will be statically compiled to all units,
 * so do not mark them as a BM_PUBLIC.
 */
char* bm_strdup(const char *s);
bool bm_resize_buffer(char **in_out_buffer, size_t *in_out_size, size_t nsize);
BM_LOG_ATTR(1, 2) char* bm_dprintf(const char *fmt, ...);
BM_LOG_ATTR(3, 0) bool bm_vrprintf(char **in_out_buffer, size_t *in_out_len, const char *fmt, va_list args);
size_t bm_strip_token(char *string, const char *token, size_t *out_next);
int bm_strupcmp(const char *hay, const char *needle);
int bm_strnupcmp(const char *hay, const char *needle, size_t len);
char* bm_strupstr(const char *hay, const char *needle);
int32_t bm_utf8_string_screen_width(const char *string);
size_t bm_utf8_rune_next(const char *string, size_t start);
size_t bm_utf8_rune_prev(const char *string, size_t start);
size_t bm_utf8_rune_width(const char *rune, uint32_t u8len);
size_t bm_utf8_rune_remove(char *string, size_t start, size_t *out_rune_width);
size_t bm_utf8_rune_insert(char **string, size_t *bufSize, size_t start, const char *rune, uint32_t u8len, size_t *out_rune_width);
size_t bm_unicode_insert(char **string, size_t *bufSize, size_t start, uint32_t unicode, size_t *out_rune_width);
bool bm_menu_item_is_selected(const struct bm_menu *menu, const struct bm_item *item);

#endif /* _BEMENU_INTERNAL_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
