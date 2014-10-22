#include "bemenu.h"

#ifndef size_t
#   include <stddef.h> /* for size_t */
#endif

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
    bool (*constructor)(void);

    /**
     * Release underlying renderer.
     */
    void (*destructor)(void);

    /**
     * Get count of displayed items by the underlying renderer.
     */
    uint32_t (*get_displayed_count)(const struct bm_menu *menu);

    /**
     * If the underlying renderer is a UI toolkit. (curses, etc...)
     * There might be possibility to get user input, and this should be thus implemented.
     */
    enum bm_key (*poll_key)(uint32_t *unicode);

    /**
     * Tells underlying renderer to draw the menu.
     */
    void (*render)(const struct bm_menu *menu);
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
    const struct bm_renderer *renderer;

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
     * Text used to filter matches.
     */
    char *filter;

    /**
     * Used as optimization.
     */
    char *old_filter;

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
     * Current filtering method in menu instance.
     */
    enum bm_filter_mode filter_mode;

    /**
     * Should selection be wrapped?
     */
    bool wrap;
};

/* library.c */
bool bm_renderer_activate(struct bm_renderer *renderer);

/* menu.c */
bool bm_menu_item_is_selected(const struct bm_menu *menu, const struct bm_item *item);

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

/* util.c */
char* bm_strdup(const char *s);
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

/* vim: set ts=8 sw=4 tw=0 :*/
