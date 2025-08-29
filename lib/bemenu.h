#ifndef _BEMENU_H_
#define _BEMENU_H_

/*
 * When using a compiler with support for GNU C extensions (GCC, Clang),
 * explicitly mark functions shared across library boundaries as visible.
 *
 * This is the default visibility and has no effect when compiling with the
 * default CFLAGS. When compiling with -fvisibility=hidden, however, this
 * exports only functions marked BM_PUBLIC and hides all others.
 *
 * https://gcc.gnu.org/wiki/Visibility
 */
#if __GNUC__ >= 4
#  define BM_PUBLIC __attribute__((visibility ("default")))
#else
#  define BM_PUBLIC
#endif

/**
 * @file bemenu.h
 *
 * Public API header.
 */

struct bm_renderer;
struct bm_menu;
struct bm_item;

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup Library
 * @brief Library functions.
 *
 * Query library version, etc...
 */

/**
 * @defgroup Menu
 * @brief Menu container.
 *
 * Holds all the items, runs logic and gets rendered.
 */

/**
 * @defgroup Item
 * @brief Item container.
 *
 * Contains properties for visual representation of item.
 */

/**
 * @addtogroup Library
 * @{ */

/**
 * @name Library Initialization
 * @{ */

/**
 * Init bemenu, loads up the renderers.
 *
 * You can force single renderer with BEMENU_RENDERER env variable,
 * and directory containing renderers with BEMENU_RENDERERS env variable.
 *
 * @return true on success, false on failure.
 */
BM_PUBLIC bool bm_init(void);

/**
 * Get list of available renderers.
 *
 * @param out_nmemb Reference to uint32_t where total count of returned renderers will be stored.
 * @return Pointer to array of bm_renderer instances.
 */
BM_PUBLIC const struct bm_renderer** bm_get_renderers(uint32_t *out_nmemb);

/** @} Library Initialization */

/**
 * @name Library Version
 * @{ */

/**
 * Get version of the library in 'major.minor.patch' format.
 *
 * @see @link http://semver.org/ Semantic Versioning @endlink
 *
 * @return Null terminated C "string" to version string.
 */
BM_PUBLIC const char* bm_version(void);

/**  @} Library Version */

/**  @} Library */

/**
 * @addtogroup Renderer
 * @{ */

/**
 * Prioritories for renderer plugins.
 */
enum bm_priorty {
    /**
     * Renderer runs in terminal.
     */
    BM_PRIO_TERMINAL,

    /**
     * Renderer runs in GUI.
     */
    BM_PRIO_GUI,
};

/**
 * Vertical position of the menu.
 */
enum bm_align {
    /**
     * Menu is at the top of the screen.
     */
    BM_ALIGN_TOP,

    /**
     * Menu is at the bottom of the screen.
     */
    BM_ALIGN_BOTTOM,

    /**
     * Menu is in the center of the screen.
     */
    BM_ALIGN_CENTER,
};

/**
 * Get name of the renderer.
 *
 * @param renderer bm_renderer instance.
 * @return Null terminated C "string" to renderer's name.
 */
BM_PUBLIC const char* bm_renderer_get_name(const struct bm_renderer *renderer);

/**
 * Get priorty of the renderer.
 *
 * @param renderer bm_renderer instance.
 * @return bm_priorty enum value.
 */
BM_PUBLIC enum bm_priorty bm_renderer_get_priorty(const struct bm_renderer *renderer);

/**
 * @} Renderer */

/**
 * @addtogroup Menu
 * @{ */

/**
 * Filter mode constants for bm_menu instance filter mode.
 *
 * @link ::bm_filter_mode BM_FILTER_MODE_LAST @endlink is provided for enumerating filter modes.
 * Using it as filter mode however provides exactly same functionality as BM_FILTER_MODE_DMENU.
 */
enum bm_filter_mode {
    BM_FILTER_MODE_DMENU,
    BM_FILTER_MODE_DMENU_CASE_INSENSITIVE,
    BM_FILTER_MODE_LAST
};

/**
 * Scrollbar display mode constants for bm_menu instance scrollbar.
 *
 * - @link ::bm_scrollbar_mode BM_SCROLLBAR_NONE @endlink means that scrollbar is not displayed.
 * - @link ::bm_scrollbar_mode BM_SCROLLBAR_ALWAYS @endlink means that scrollbar is displayed always.
 * - @link ::bm_scrollbar_mode BM_SCROLLBAR_AUTOHIDE @endlink means that scrollbar is only displayed when there are more items than lines.
 *
 * @link ::bm_scrollbar_mode BM_SCROLLBAR_LAST @endlink is provided for enumerating scrollbar modes.
 * Using it as scrollbar mode however provides exactly same functionality as BM_SCROLLBAR_NONE.
 */
enum bm_scrollbar_mode {
    BM_SCROLLBAR_NONE,
    BM_SCROLLBAR_ALWAYS,
    BM_SCROLLBAR_AUTOHIDE,
    BM_SCROLLBAR_LAST,
};

/**
 * Password display mode constants for bm_menu instance filter text.
 *
 * - @link ::bm_password_mode BM_PASSWORD_NONE @endlink means that filter text displays what the user inputs(default behavior).
 * - @link ::bm_password_mode BM_PASSWORD_HIDE @endlink means that filter text is not displayed at all.
 * - @link ::bm_password_mode BM_PASSWORD_INDICATOR @endlink means that filter text is replaced with asterisks.
 */
enum bm_password_mode {
    BM_PASSWORD_NONE,
    BM_PASSWORD_HIDE,
    BM_PASSWORD_INDICATOR,
};

/**
 * Vertical menu display mode constants for bm_menu instance lines.
 *
 * - @link ::bm_lines_mode BM_LINES_DOWN @endlink means that the vertical menu lines will go downwards.
 * - @link ::bm_lines_mode BM_LINES_UP @endlink means that the vertical menu lines will go upwards
 */
enum bm_lines_mode {
    BM_LINES_DOWN,
    BM_LINES_UP,
};

/**
 * Result constants for the menu run functions.
 *
 * - @link ::bm_run_result BM_RUN_RESULT_RUNNING @endlink means that menu is running and thus should be still renderer && ran.
 * - @link ::bm_run_result BM_RUN_RESULT_SELECTED @endlink means that menu was closed and items were selected.
 * - @link ::bm_run_result BM_RUN_RESULT_CANCEL @endlink means that menu was closed and selection was canceled.
 */
enum bm_run_result {
    BM_RUN_RESULT_RUNNING,
    BM_RUN_RESULT_SELECTED,
    BM_RUN_RESULT_CANCEL,
    BM_RUN_RESULT_CUSTOM_1,
    BM_RUN_RESULT_CUSTOM_2,
    BM_RUN_RESULT_CUSTOM_3,
    BM_RUN_RESULT_CUSTOM_4,
    BM_RUN_RESULT_CUSTOM_5,
    BM_RUN_RESULT_CUSTOM_6,
    BM_RUN_RESULT_CUSTOM_7,
    BM_RUN_RESULT_CUSTOM_8,
    BM_RUN_RESULT_CUSTOM_9,
    BM_RUN_RESULT_CUSTOM_10,
};

/**
 * Key constants.
 *
 * @link ::bm_key BM_KEY_LAST @endlink is provided for enumerating keys.
 */
enum bm_key {
    BM_KEY_NONE,
    BM_KEY_UP,
    BM_KEY_DOWN,
    BM_KEY_LEFT,
    BM_KEY_RIGHT,
    BM_KEY_HOME,
    BM_KEY_END,
    BM_KEY_PAGE_UP,
    BM_KEY_PAGE_DOWN,
    BM_KEY_SHIFT_PAGE_UP,
    BM_KEY_SHIFT_PAGE_DOWN,
    BM_KEY_BACKSPACE,
    BM_KEY_DELETE,
    BM_KEY_LINE_DELETE_LEFT,
    BM_KEY_LINE_DELETE_RIGHT,
    BM_KEY_WORD_DELETE,
    BM_KEY_PASTE_PRIMARY,
    BM_KEY_PASTE_CLIPBOARD,
    BM_KEY_TAB,
    BM_KEY_SHIFT_TAB,
    BM_KEY_ESCAPE,
    BM_KEY_RETURN,
    BM_KEY_SHIFT_RETURN,
    BM_KEY_CONTROL_RETURN,
    BM_KEY_CUSTOM_1,
    BM_KEY_CUSTOM_2,
    BM_KEY_CUSTOM_3,
    BM_KEY_CUSTOM_4,
    BM_KEY_CUSTOM_5,
    BM_KEY_CUSTOM_6,
    BM_KEY_CUSTOM_7,
    BM_KEY_CUSTOM_8,
    BM_KEY_CUSTOM_9,
    BM_KEY_CUSTOM_10,
    BM_KEY_UNICODE,
    BM_KEY_LAST
};

enum bm_pointer_key {
    BM_POINTER_KEY_NONE,
    BM_POINTER_KEY_PRIMARY,
};

enum bm_pointer_axis {
    BM_POINTER_AXIS_VERTICAL = 0,
    BM_POINTER_AXIS_HORIZONTAL = 1,
};

enum bm_pointer_event_mask {
    POINTER_EVENT_ENTER = 1 << 1,
    POINTER_EVENT_LEAVE = 1 << 2,
    POINTER_EVENT_MOTION = 1 << 3,
    POINTER_EVENT_BUTTON = 1 << 4,
    POINTER_EVENT_AXIS = 1 << 5,
    POINTER_EVENT_AXIS_SOURCE = 1 << 6,
    POINTER_EVENT_AXIS_STOP = 1 << 7,
    POINTER_EVENT_AXIS_DISCRETE = 1 << 8,
};

enum bm_pointer_state_mask {
    POINTER_STATE_RELEASED,
    POINTER_STATE_PRESSED,
};

struct bm_pointer {
    uint32_t event_mask;
    uint32_t pos_x, pos_y;
    uint32_t button, state;
    uint32_t time;
    struct {
        bool valid;
        int32_t value;
        int32_t discrete;
    } axes[2];
    uint32_t axis_source;
};

enum bm_touch_event_mask {
    TOUCH_EVENT_DOWN = 1 << 0,
    TOUCH_EVENT_UP = 1 << 1,
    TOUCH_EVENT_MOTION = 1 << 2,
    TOUCH_EVENT_CANCEL = 1 << 3,
    TOUCH_EVENT_SHAPE = 1 << 4,
    TOUCH_EVENT_ORIENTATION = 1 << 5,
};

struct bm_touch_point {
    bool valid;
    int32_t id;
    uint32_t event_mask;
    int32_t start_x, start_y;
    int32_t pos_x, pos_y;
    uint32_t major, minor;
    uint32_t orientation;
};

struct bm_touch {
    uint32_t time;
    uint32_t serial;
    struct bm_touch_point points[2];
};

enum bm_event_feedback_mask {
    TOUCH_WILL_SCROLL_UP = 1 << 1,
    TOUCH_WILL_SCROLL_DOWN = 1 << 2,
    TOUCH_WILL_SCROLL_FIRST = 1 << 3,
    TOUCH_WILL_SCROLL_LAST = 1 << 4,
    TOUCH_WILL_CANCEL = 1 << 5,
};

/**
 * Colorable element constants.
 *
 * @link ::bm_color BM_COLOR_LAST @endlink is provided for enumerating colors.
 */
enum bm_color {
    BM_COLOR_TITLE_BG,
    BM_COLOR_TITLE_FG,
    BM_COLOR_FILTER_BG,
    BM_COLOR_FILTER_FG,
    BM_COLOR_CURSOR_BG,
    BM_COLOR_CURSOR_FG,
    BM_COLOR_ITEM_BG,
    BM_COLOR_ITEM_FG,
    BM_COLOR_HIGHLIGHTED_BG,
    BM_COLOR_HIGHLIGHTED_FG,
    BM_COLOR_FEEDBACK_BG,
    BM_COLOR_FEEDBACK_FG,
    BM_COLOR_SELECTED_BG,
    BM_COLOR_SELECTED_FG,
    BM_COLOR_ALTERNATE_BG,
    BM_COLOR_ALTERNATE_FG,
    BM_COLOR_SCROLLBAR_BG,
    BM_COLOR_SCROLLBAR_FG,
    BM_COLOR_BORDER,
    BM_COLOR_LAST
};


/**
 * Key bindings
 */
enum bm_key_binding {
    BM_KEY_BINDING_DEFAULT,
    BM_KEY_BINDING_VIM,
};


/**
 * @name Menu Memory
 * @{ */

/**
 * Create new bm_menu instance.
 *
 * If **NULL** is used as renderer, auto-detection will be used or the renderer with the name pointed by BEMENU_BACKEND env variable.
 * It's good idea to use NULL, if you want user to have control over the renderer with this env variable.
 *
 * @param renderer Name of renderer to be used for this instance, pass **NULL** for auto-detection.
 * @return bm_menu for new menu instance, **NULL** if creation failed.
 */
BM_PUBLIC struct bm_menu* bm_menu_new(const char *renderer);

/**
 * Release bm_menu instance.
 *
 * @param menu bm_menu instance to be freed from memory.
 */
BM_PUBLIC void bm_menu_free(struct bm_menu *menu);

/**
 * Release items inside bm_menu instance.
 *
 * @param menu bm_menu instance which items will be freed from memory.
 */
BM_PUBLIC void bm_menu_free_items(struct bm_menu *menu);

/**  @} Menu Memory */

/**
 * @name Menu Properties
 * @{ */

/**
 * Get the renderer from the bm_menu instance.
 *
 * @param menu bm_menu instance which renderer to get.
 * @return Pointer to bm_renderer instance.
 */
BM_PUBLIC const struct bm_renderer* bm_menu_get_renderer(struct bm_menu *menu);

/**
 * Set userdata pointer to bm_menu instance.
 * Userdata will be carried unmodified by the instance.
 *
 * @param menu bm_menu instance where to set userdata pointer.
 * @param userdata Pointer to userdata.
 */
BM_PUBLIC void bm_menu_set_userdata(struct bm_menu *menu, void *userdata);

/**
 * Get userdata pointer from bm_menu instance.
 *
 * @param menu bm_menu instance which userdata pointer to get.
 * @return Pointer for unmodified userdata.
 */
BM_PUBLIC void* bm_menu_get_userdata(struct bm_menu *menu);

/**
 * Set highlight prefix.
 * This is shown on vertical list mode only.
 *
 * @param menu bm_menu instance where to set highlight prefix.
 * @param prefix Null terminated C "string" to act as prefix for highlighted item. May be set **NULL** for none.
 */
BM_PUBLIC void bm_menu_set_prefix(struct bm_menu *menu, const char *prefix);

/**
 * Get highlight prefix.
 *
 * @param menu bm_menu instance where to get highlight prefix.
 * @param Const pointer to current highlight prefix, may be **NULL** if empty.
 */
BM_PUBLIC const char* bm_menu_get_prefix(struct bm_menu *menu);

/**
 * Set filter text to bm_menu instance.
 *
 * The cursor will be automatically placed at the end of the new filter text.
 *
 * @param menu bm_menu instance where to set filter.
 * @param filter Null terminated C "string" to act as filter. May be set **NULL** for none.
 */
BM_PUBLIC void bm_menu_set_filter(struct bm_menu *menu, const char *filter);

/**
 * Get filter text from bm_menu instance.
 *
 * @param menu bm_menu instance where to get filter.
 * @return Const pointer to current filter text, may be **NULL** if empty.
 */
BM_PUBLIC const char* bm_menu_get_filter(struct bm_menu *menu);

/**
 * Set active filter mode to bm_menu instance.
 *
 * @param menu bm_menu instance where to set filter mode.
 * @param mode bm_filter_mode constant.
 */
BM_PUBLIC void bm_menu_set_filter_mode(struct bm_menu *menu, enum bm_filter_mode mode);

/**
 * Get active filter mode from bm_menu instance.
 *
 * @param menu bm_menu instance where to get filter mode.
 * @return bm_filter_mode constant.
 */
BM_PUBLIC enum bm_filter_mode bm_menu_get_filter_mode(const struct bm_menu *menu);

/**
 * Set amount of max vertical lines to be shown.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set max vertical line amount.
 * @param lines 0 for single line layout, > 0 to show that many lines.
 */
BM_PUBLIC void bm_menu_set_lines(struct bm_menu *menu, uint32_t lines);

/**
 * Get amount of max vertical lines to be shown.
 *
 * @param menu bm_menu instance where to get max vertical line amount.
 * @return uint32_t for max amount of vertical lines to be shown.
 */
BM_PUBLIC uint32_t bm_menu_get_lines(struct bm_menu *menu);

/**
 * Set the direction in which lines should go after the filter text.
 *
 * @param menu bm_menu instance where to set lines direction.
 * @param mode bm_lines_mode constant.
 */
BM_PUBLIC void bm_menu_set_lines_mode(struct bm_menu *menu, enum bm_lines_mode mode);

/**
 * Get the direction in which lines should go after the filter text.
 *
 * @param menu bm_menu instance where to get the lines direction.
 * @return bm_lines_mode constant
 */
BM_PUBLIC enum bm_lines_mode bm_menu_get_lines_mode(struct bm_menu *menu);

/**
 * Set selection wrapping on/off.
 *
 * @param menu bm_menu instance where to toggle selection wrapping.
 * @param wrap true/false.
 */
BM_PUBLIC void bm_menu_set_wrap(struct bm_menu *menu, bool wrap);

/**
 * Get selection wrapping state.
 *
 * @param menu bm_menu instance where to get selection wrapping state.
 * @return int for wrap state.
 */
BM_PUBLIC bool bm_menu_get_wrap(const struct bm_menu *menu);

/**
 * Set title to bm_menu instance.
 *
 * @param menu bm_menu instance where to set title.
 * @param title C "string" to set as title, can be **NULL** for empty title.
 * @return true if set was succesful, false if out of memory.
 */
BM_PUBLIC bool bm_menu_set_title(struct bm_menu *menu, const char *title);

/**
 * Get title from bm_menu instance.
 *
 * @param menu bm_menu instance where to get title from.
 * @return Pointer to null terminated C "string", can be **NULL** for empty title.
 */
BM_PUBLIC const char* bm_menu_get_title(const struct bm_menu *menu);

/**
 * Set font description to bm_menu instance.
 *
 * @param menu bm_menu instance where to set font.
 * @param font C "string" for a **pango style font description**, can be **NULL** for default (Terminus 9).
 * @return true if set was succesful, false if out of memory.
 */
BM_PUBLIC bool bm_menu_set_font(struct bm_menu *menu, const char *font);

/**
 * Get font description from bm_menu instance.
 *
 * @param menu bm_menu instance where to get font description from.
 * @return Pointer to null terminated C "string".
 */
BM_PUBLIC const char* bm_menu_get_font(const struct bm_menu *menu);

/**
 * Set size of line in pixels.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set line height.
 * @param line_height 0 for default line height, > 0 for that many pixels.
 */
BM_PUBLIC void bm_menu_set_line_height(struct bm_menu *menu, uint32_t line_height);

/**
 * Get size of line in pixels.
 *
 * @param menu bm_menu instance where to get line height.
 * @return uint32_t for max amount of vertical lines to be shown.
 */
BM_PUBLIC uint32_t bm_menu_get_line_height(struct bm_menu *menu);

/**
 * Set height of cursor in pixels.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set cursor height.
 * @param cursor_height 0 for default cursor height, > 0 for that many pixels.
 */
BM_PUBLIC void bm_menu_set_cursor_height(struct bm_menu *menu, uint32_t cursor_height);

/**
 * Get height of cursor in pixels.
 *
 * @param menu bm_menu instance where to get cursor height.
 * @return uint32_t for max amount of vertical cursors to be shown.
 */
BM_PUBLIC uint32_t bm_menu_get_cursor_height(struct bm_menu *menu);

/**
 * Set width of cursor in pixels.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set cursor height.
 * @param cursor_width 0 for default cursor height, > 0 for that many pixels.
 */
BM_PUBLIC void bm_menu_set_cursor_width(struct bm_menu *menu, uint32_t cursor_width);

/**
 * Get width of cursor in pixels.
 *
 * @param menu bm_menu instance where to get cursor height.
 * @return uint32_t for max amount of vertical cursors to be shown.
 */
BM_PUBLIC uint32_t bm_menu_get_cursor_width(struct bm_menu *menu);

/**
 * Set horizontal padding in pixels.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set horizontal padding.
 * @return uint32_t for horizontal padding
 */
BM_PUBLIC void bm_menu_set_hpadding(struct bm_menu *menu, uint32_t hpadding);

/**
 * Get horizontal padding in pixels.
 * Some renderers such as ncurses may ignore this when it does not make sense.
 *
 * @param menu bm_menu instance where to set horizontal padding.
 * @return uint32_t for horizontal padding
 */
BM_PUBLIC uint32_t bm_menu_get_hpadding(struct bm_menu *menu);

/**
 * Get with of menu in pixels.
 *
 * @param menu bm_menu instance where to get line height.
 * @return uint32_t for max amount the menu height.
 */
BM_PUBLIC uint32_t bm_menu_get_height(struct bm_menu *menu);

/**
 * Get with of menu in pixels.
 *
 * @param menu bm_menu instance where to get line width.
 * @return uint32_t for max amount the menu width.
 */
BM_PUBLIC uint32_t bm_menu_get_width(struct bm_menu *menu);

/**
 * Set the size of the border for the bar
 *
 * @param menu bm_menu to get border size from.
 */

BM_PUBLIC void bm_menu_set_border_size(struct bm_menu* menu, double border_size);

/**
 * Get the size of the border for the bar
 *
 * @param menu bm_menu to get border size from.
 * @return border size of the menu.
 */

BM_PUBLIC double bm_menu_get_border_size(struct bm_menu* menu);

/**
 * Set the radius of the border for the bar
 *
 * @param menu bm_menu to get border radius from.
 */

BM_PUBLIC void bm_menu_set_border_radius(struct bm_menu* menu, double border_radius);

/**
 * Get the size of the border for the bar
 *
 * @param menu bm_menu to get border radius from.
 * @return border radius of the menu.
 */

BM_PUBLIC double bm_menu_get_border_radius(struct bm_menu* menu);

/**
 * Set a hexadecimal color for element.
 *
 * @param menu bm_menu instance where to set color.
 * @param color bm_color type.
 * @param hex Color in hexadecimal format starting with '#'.
 * @return true if set was succesful, false if out of memory.
 */
BM_PUBLIC bool bm_menu_set_color(struct bm_menu *menu, enum bm_color color, const char *hex);

/**
 * Get hexadecimal color for element.
 *
 * @param menu bm_menu instance where to get color from.
 * @param color bm_color type.
 * @return Pointer to null terminated C "string".
 */
BM_PUBLIC const char* bm_menu_get_color(const struct bm_menu *menu, enum bm_color color);

/**
 * Set scrollbar display mode.
 *
 * @param menu bm_menu instance to set scrollbar for.
 * @param mode bm_scrollbar_mode constant.
 */
BM_PUBLIC void bm_menu_set_scrollbar(struct bm_menu *menu, enum bm_scrollbar_mode mode);

/**
 * Set fixed height mode of the bar.
 *
 * @param menu bm_menu to set the fixed height for.
 * @param mode to be set.
 */

BM_PUBLIC void bm_menu_set_fixed_height(struct bm_menu *menu, bool mode);

/**
 * Get the fixed height mode of the bar.
 *
 * @param menu bm_menu to get the fixed height from.
 * @return current fixed height mode
 */

BM_PUBLIC bool bm_menu_get_fixed_height(struct bm_menu *menu);

/**
 * Return current scrollbar display mode.
 *
 * @param menu bm_menu instance where to get scrollbar display state from.
 * @return bm_scrollbar_mode constant.
 */
BM_PUBLIC enum bm_scrollbar_mode bm_menu_get_scrollbar(struct bm_menu *menu);

/**
 * Set the counter display mode.
 *
 * @param menu bm_menu to set the counter for.
 * @param display mode to be set.
 */

BM_PUBLIC void bm_menu_set_counter(struct bm_menu *menu, bool mode);

/**
 * Get the counter display mode.
 *
 * @param menu bm_menu to get the counter from.
 * @return current counter display mode
 */

BM_PUBLIC bool bm_menu_get_counter(struct bm_menu *menu);

/**
 * Set vim escape exit mode of the bar.
 *
 * @param menu bm_menu to set the vim escape exit mode for.
 * @param mode to be set.
 */

BM_PUBLIC void bm_menu_set_vim_esc_exits(struct bm_menu *menu, bool mode);

/**
 * Get the vim escape exit mode of the bar.
 *
 * @param menu bm_menu to get the vim escape exit mode from.
 * @param mode current vim escape exit mode
 */

BM_PUBLIC bool bm_menu_get_vim_esc_exits(struct bm_menu *menu);

/**
 * Set the vertical alignment of the bar.
 *
 * @param menu bm_menu to set alignment for.
 * @param align alignment to set
 */
BM_PUBLIC void bm_menu_set_align(struct bm_menu *menu, enum bm_align align);

/**
 * Get the vertical alignment of the bar.
 *
 * @param menu bm_menu to get alignment for.
 * @return alignment for the menu
 */

BM_PUBLIC enum bm_align bm_menu_get_align(struct bm_menu *menu);

/**
 * Set the vertical/y offset of the window.
 *
 * @param menu bm_menu to set offset for.
 * @param y_offset offset to set.
 */
BM_PUBLIC void bm_menu_set_y_offset(struct bm_menu *menu, int32_t y_offset);

/**
 * Get the vertical/y offset of the window.
 *
 * @param menu bm_menu to get y offset from.
 * @return y offset of the menu.
 */

BM_PUBLIC int32_t bm_menu_get_y_offset(struct bm_menu *menu);

/**
 * Set the horizontal margin and the relative width factor of the bar.
 *
 * @param menu bm_menu to set horizontal margin for.
 * @param margin margin to set.
 * @param factor factor to set.
 */

BM_PUBLIC void bm_menu_set_width(struct bm_menu *menu, uint32_t margin, float factor);

/**
 * Get the horizontal margin of the bar.
 *
 * @param menu bm_menu to get horizontal margin from.
 * @return horizontal margin of the menu
 */

BM_PUBLIC uint32_t bm_menu_get_hmargin_size(struct bm_menu *menu);

/**
 * Get the relative width factor of the bar.
 *
 * @param menu bm_menu to get relative width factor from.
 * @return relative width factor of the menu.
 */

BM_PUBLIC float bm_menu_get_width_factor(struct bm_menu *menu);

/**
 * Display menu at monitor index.
 * Indices start at 0, a value of -1 can be passed for the active monitor (default).
 * If index is more than amount of monitors, the monitor with highest index will be selected.
 *
 * @param menu bm_menu instance to set monitor for.
 * @param monitor Monitor index starting from 0, or -1 for the active monitor.
 */
BM_PUBLIC void bm_menu_set_monitor(struct bm_menu *menu, int32_t monitor);

/**
 * Display menu with monitor_name.
 * Only works for Wayland.
 *
 * @param menu bm_menu instance to set monitor for.
 * @param monitor_name Monitor name passed in.
 */
BM_PUBLIC void bm_menu_set_monitor_name(struct bm_menu *menu, char *monitor_name);

/**
 * Return index for current monitor.
 *
 * @param menu bm_menu instance where to get current monitor from.
 * @return Monitor index starting from 1.
 */
BM_PUBLIC uint32_t bm_menu_get_monitor(struct bm_menu *menu);

/**
 * Tell renderer to grab keyboard.
 * This only works with x11 renderer.
 *
 * @param menu bm_menu instance to set grab for.
 * @param grab true for grab, false for ungrab.
 */
BM_PUBLIC void bm_menu_grab_keyboard(struct bm_menu *menu, bool grab);

/**
 * Is keyboard grabbed for bm_menu?
 *
 * @param menu bm_menu instance where to check grab status from.
 * @return true if grabbed, false if not.
 */
BM_PUBLIC bool bm_menu_is_keyboard_grabbed(struct bm_menu *menu);

/**
 * Tell the renderer to position the menu that it can overlap panels.
 */
BM_PUBLIC void bm_menu_set_panel_overlap(struct bm_menu *menu, bool overlap);

/**
 * Set current password mode.
 *
 * @param menu bm_menu instance to set password mode for.
 * @param password BM_PASSWORD_NONE for default behavior(display what user inputs), BM_PASSWORD_HIDE for no filter text to be displayed, BM_PASSWORD_INDICATOR to replace input with asterisks.
 */
BM_PUBLIC void bm_menu_set_password(struct bm_menu *menu, enum bm_password_mode mode);

/**
 * Space entries with title.
 *
 * @param menu bm_menu instance to set password mode for.
 * @param spacing true to enable title spacing
 */
BM_PUBLIC void bm_menu_set_spacing(struct bm_menu *menu, bool spacing);

/**
 * Is password mode activated and input hidden?
 *
 * @param menu bm_menu instance where to get password mode from.
 * @return BM_PASSWORD_NONE is default behavior(display what user inputs), BM_PASSWORD_HIDE is no filter text is being displayed, BM_PASSWORD_INDICATOR is replacing input with asterisks.
 */
BM_PUBLIC enum bm_password_mode bm_menu_get_password(struct bm_menu *menu);


/**
 * Specify the key bindings that should be used. 
 *
 * @param menu bm_menu instance to set the key binding on.
 * @param key_binding binding that should be used.
 */
BM_PUBLIC void bm_menu_set_key_binding(struct bm_menu *menu, enum bm_key_binding);


/**  @} Properties */

/**
 * @name Menu Items
 * @{ */

/**
 * Add item to bm_menu instance at specific index.
 *
 * @param menu bm_menu instance where item will be added.
 * @param item bm_item instance to add.
 * @param index Index where item will be added.
 * @return true on successful add, false on failure.
 */
BM_PUBLIC bool bm_menu_add_item_at(struct bm_menu *menu, struct bm_item *item, uint32_t index);

/**
 * Add item to bm_menu instance.
 *
 * @param menu bm_menu instance where item will be added.
 * @param item bm_item instance to add.
 * @return true on successful add, false on failure.
 */
BM_PUBLIC bool bm_menu_add_item(struct bm_menu *menu, struct bm_item *item);

/**
 * Remove item from bm_menu instance at specific index.
 *
 * @warning The item won't be freed, use bm_item_free to do that.
 *
 * @param menu bm_menu instance from where item will be removed.
 * @param index Index of item to remove.
 * @return true on successful add, false on failure.
 */
BM_PUBLIC bool bm_menu_remove_item_at(struct bm_menu *menu, uint32_t index);

/**
 * Remove item from bm_menu instance.
 *
 * @warning The item won't be freed, use bm_item_free to do that.
 *
 * @param menu bm_menu instance from where item will be removed.
 * @param item bm_item instance to remove.
 * @return true on successful add, false on failure.
 */
BM_PUBLIC bool bm_menu_remove_item(struct bm_menu *menu, struct bm_item *item);

/**
 * Highlight item in menu by index.
 *
 * @param menu bm_menu instance from where to highlight item.
 * @param index Index of item to highlight.
 * @return true on successful highlight, false on failure.
 */
BM_PUBLIC bool bm_menu_set_highlighted_index(struct bm_menu *menu, uint32_t index);

/**
 * Highlight item in menu.
 *
 * @param menu bm_menu instance from where to highlight item.
 * @param item bm_item instance to highlight.
 * @return true on successful highlight, false on failure.
 */
BM_PUBLIC bool bm_menu_set_highlighted_item(struct bm_menu *menu, struct bm_item *item);

/**
 * Get highlighted item from bm_menu instance.
 *
 * @warning The pointer returned by this function may be invalid after items change.
 *
 * @param menu bm_menu instance from where to get highlighted item.
 * @return Selected bm_item instance, **NULL** if none highlighted.
 */
BM_PUBLIC struct bm_item* bm_menu_get_highlighted_item(const struct bm_menu *menu);

/**
 * Set selected items to bm_menu instance.
 *
 * @param menu bm_menu instance where items will be set.
 * @param items Array of bm_item pointers to set.
 * @param nmemb Total count of items in array.
 * @return true on successful set, false on failure.
 */
BM_PUBLIC bool bm_menu_set_selected_items(struct bm_menu *menu, struct bm_item **items, uint32_t nmemb);

/**
 * Get selected items from bm_menu instance.
 *
 * @warning The pointer returned by this function may be invalid after selection or items change.
 *
 * @param menu bm_menu instance from where to get selected items.
 * @param out_nmemb Reference to uint32_t where total count of returned items will be stored.
 * @return Pointer to array of bm_item pointers.
 */
BM_PUBLIC struct bm_item** bm_menu_get_selected_items(const struct bm_menu *menu, uint32_t *out_nmemb);

/**
 * Set items to bm_menu instance.
 * Will replace all the old items on bm_menu instance.
 *
 * If items is **NULL**, or nmemb is zero, all items will be freed from the menu.
 *
 * @param menu bm_menu instance where items will be set.
 * @param items Array of bm_item pointers to set.
 * @param nmemb Total count of items in array.
 * @return true on successful set, false on failure.
 */
BM_PUBLIC bool bm_menu_set_items(struct bm_menu *menu, const struct bm_item **items, uint32_t nmemb);

/**
 * Get items from bm_menu instance.
 *
 * @warning The pointer returned by this function may be invalid after removing or adding new items.
 *
 * @param menu bm_menu instance from where to get items.
 * @param out_nmemb Reference to uint32_t where total count of returned items will be stored.
 * @return Pointer to array of bm_item pointers.
 */
BM_PUBLIC struct bm_item** bm_menu_get_items(const struct bm_menu *menu, uint32_t *out_nmemb);

/**
 * Get filtered (displayed) items from bm_menu instance.
 *
 * @warning The pointer returned by this function _will_ be invalid when menu internally filters its list again.
 *          Do not store this pointer.
 *
 * @param menu bm_menu instance from where to get filtered items.
 * @param out_nmemb Reference to uint32_t where total count of returned items will be stored.
 * @return Pointer to array of bm_item pointers.
 */
BM_PUBLIC struct bm_item** bm_menu_get_filtered_items(const struct bm_menu *menu, uint32_t *out_nmemb);

/**  @} Menu Items */

/**
 * @name Menu Logic
 * @{ */

/**
 * Render bm_menu instance using chosen renderer.
 *
 * This function may block on **wayland** and **x11** renderer.
 *
 * @param menu bm_menu instance to be rendered.
 */
BM_PUBLIC bool bm_menu_render(struct bm_menu *menu);

/**
 * Trigger filtering of menu manually.
 * This is useful when adding new items and want to dynamically see them filtered.
 *
 * Do note that filtering might be heavy, so you should only call it after batch manipulation of items.
 * Not after manipulation of each single item.
 *
 * @param menu bm_menu instance which to filter.
 */
BM_PUBLIC void bm_menu_filter(struct bm_menu *menu);

/**
 * Poll key and unicode from underlying UI toolkit.
 *
 * This function will block on **curses** renderer.
 *
 * @param menu bm_menu instance from which to poll.
 * @param out_unicode Reference to uint32_t.
 * @return bm_key for polled key.
 */
BM_PUBLIC enum bm_key bm_menu_poll_key(struct bm_menu *menu, uint32_t *out_unicode);

/**
 * Poll pointer and unicode from underlying UI toolkit.
 *
 * This function will block on **curses** renderer.
 *
 * @param menu bm_menu instance from which to poll.
 * @return bm_pointer for polled pointer.
 */
BM_PUBLIC struct bm_pointer bm_menu_poll_pointer(struct bm_menu *menu);

/**
 * Poll touch and unicode from underlying UI toolkit.
 *
 * This function will block on **curses** renderer.
 *
 * @param menu bm_menu instance from which to poll.
 * @return bm_touch for polled touch.
 */
BM_PUBLIC struct bm_touch bm_menu_poll_touch(struct bm_menu *menu);

/**
 * Enforce a release of the touches.
 *
 * @param menu bm_menu instance from which to poll.
 */
BM_PUBLIC void bm_menu_release_touch(struct bm_menu *menu);

/**
 * Advances menu logic with a key and unicode as input.
 *
 * @param menu bm_menu instance to be advanced.
 * @param key Key input that will advance menu logic.
 * @param unicode Unicode input that will advance menu logic.
 * @return bm_run_result for menu state.
 */
BM_PUBLIC enum bm_run_result bm_menu_run_with_key(struct bm_menu *menu, enum bm_key key, uint32_t unicode);

/**
 * Advances menu logic with a pointer event.
 *
 * @param menu bm_menu instance to be advanced.
 * @param pointer Pointer input that will advance menu logic.
 * @return bm_run_result for menu state.
 */
BM_PUBLIC enum bm_run_result bm_menu_run_with_pointer(struct bm_menu *menu, struct bm_pointer pointer);

/**
 * Advances menu logic with a touch event.
 *
 * @param menu bm_menu instance to be advanced.
 * @param touch Touch input that will advance menu logic.
 * @return bm_run_result for menu state.
 */
BM_PUBLIC enum bm_run_result bm_menu_run_with_touch(struct bm_menu *menu, struct bm_touch touch);

/**
 * Advances menu logic with various events.
 *
 * @param menu bm_menu instance to be advanced.
 * @param key Key input that will advance menu logic.
 * @param pointer Pointer input that will advance menu logic.
 * @param touch Touch input that will advance menu logic.
 * @param unicode Unicode input that will advance menu logic.
 * @return bm_run_result for menu state.
 */
BM_PUBLIC enum bm_run_result bm_menu_run_with_events(struct bm_menu *menu, enum bm_key key, struct bm_pointer pointer, struct bm_touch touch, uint32_t unicode);

/**  @} Menu Logic */

/**  @} Menu */

/**
 * @addtogroup Item
 * @{ */

/**
 * @name Item Memory
 * @{ */

/**
 * Allocate a new item.
 *
 * @param text Pointer to null terminated C "string", can be **NULL** for empty text.
 * @return bm_item for new item instance, **NULL** if creation failed.
 */
BM_PUBLIC struct bm_item* bm_item_new(const char *text);

/**
 * Release bm_item instance.
 *
 * @param item bm_item instance to be freed from memory.
 */
BM_PUBLIC void bm_item_free(struct bm_item *item);

/**  @} Item Memory */

/**
 * @name Item Properties
 * @{ */

/**
 * Set userdata pointer to bm_item instance.
 * Userdata will be carried unmodified by the instance.
 *
 * @param item bm_item instance where to set userdata pointer.
 * @param userdata Pointer to userdata.
 */
BM_PUBLIC void bm_item_set_userdata(struct bm_item *item, void *userdata);

/**
 * Get userdata pointer from bm_item instance.
 *
 * @param item bm_item instance which userdata pointer to get.
 * @return Pointer for unmodified userdata.
 */
BM_PUBLIC void* bm_item_get_userdata(struct bm_item *item);

/**
 * Set text to bm_item instance.
 *
 * @param item bm_item instance where to set text.
 * @param text C "string" to set as text, can be **NULL** for empty text.
 * @return true if set was succesful, false if out of memory.
 */
BM_PUBLIC bool bm_item_set_text(struct bm_item *item, const char *text);

/**
 * Get text from bm_item instance.
 *
 * @param item bm_item instance where to get text from.
 * @return Pointer to null terminated C "string", can be **NULL** for empty text.
 */
BM_PUBLIC const char* bm_item_get_text(const struct bm_item *item);

/**  @} Item Properties */

/**  @} Item */

#endif /* _BEMENU_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
