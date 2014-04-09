/**
 * @file bemenu.h
 *
 * Public header
 */

/**
 * Draw mode constants for bmMenu instance draw mode.
 *
 * BM_DRAW_MODE_LAST is provided for enumerating draw modes.
 * Instancing with it however provides exactly same functionality as BM_DRAW_MODE_NONE.
 */
typedef enum bmDrawMode {
    BM_DRAW_MODE_NONE,
    BM_DRAW_MODE_CURSES,
    BM_DRAW_MODE_LAST
} bmDrawMode;

/**
 * Filter mode constants for bmMenu instance filter mode.
 *
 * BM_FILTER_MODE_LAST is provided for enumerating filter modes.
 * Using it as filter mode however provides exactly same functionality as BM_FILTER_MODE_DMENU.
 */
typedef enum bmFilterMode {
    BM_FILTER_MODE_DMENU,
    BM_FILTER_MODE_DMENU_CASE_INSENSITIVE,
    BM_FILTER_MODE_LAST
} bmFilterMode;

/**
 * Result constants from bmMenuRunWithKey function.
 *
 * BM_RUN_RESULT_RUNNING means that menu is running and thus should be still renderer && ran.
 * BM_RUN_RESULT_SELECTED means that menu was closed and items were selected.
 * BM_RUN_RESULT_CANCEL means that menu was closed and selection was canceled.
 */
typedef enum bmRunResult {
    BM_RUN_RESULT_RUNNING,
    BM_RUN_RESULT_SELECTED,
    BM_RUN_RESULT_CANCEL,
} bmRunResult;

/**
 * Key constants.
 *
 * BM_KEY_LAST is provided for enumerating keys.
 */
typedef enum bmKey {
    BM_KEY_NONE,
    BM_KEY_UP,
    BM_KEY_DOWN,
    BM_KEY_LEFT,
    BM_KEY_RIGHT,
    BM_KEY_HOME,
    BM_KEY_END,
    BM_KEY_PAGE_UP,
    BM_KEY_PAGE_DOWN,
    BM_KEY_BACKSPACE,
    BM_KEY_DELETE,
    BM_KEY_LINE_DELETE_LEFT,
    BM_KEY_LINE_DELETE_RIGHT,
    BM_KEY_WORD_DELETE,
    BM_KEY_TAB,
    BM_KEY_ESCAPE,
    BM_KEY_RETURN,
    BM_KEY_UNICODE,
    BM_KEY_LAST
} bmKey;

typedef struct _bmMenu bmMenu;
typedef struct _bmItem bmItem;

/**
 * Create new bmMenu instance.
 *
 * @param drawMode Render method to be used for this menu instance.
 * @return bmMenu for new menu instance, NULL if creation failed.
 */
bmMenu* bmMenuNew(bmDrawMode drawMode);

/**
 * Release bmMenu instance.
 *
 * @param menu bmMenu instance to be freed from memory.
 */
void bmMenuFree(bmMenu *menu);

/**
 * Release items inside bmMenu instance.
 *
 * @param menu bmMenu instance which items will be freed from memory.
 */
void bmMenuFreeItems(bmMenu *menu);

/**
 * Set active filter mode to bmMenu instance.
 *
 * @param menu bmMenu instance where to set filter mode.
 * @param mode bmFilterMode constant.
 */
void bmMenuSetFilterMode(bmMenu *menu, bmFilterMode mode);

/**
 * Get active filter mode from bmMenu instance.
 *
 * @param menu bmMenu instance where to get filter mode.
 * @return bmFilterMode constant.
 */
bmFilterMode bmMenuGetFilterMode(const bmMenu *menu);

/**
 * Set title to bmMenu instance.
 *
 * @param menu bmMenu instance where to set title.
 * @param title C "string" to set as title, can be NULL for empty title.
 */
int bmMenuSetTitle(bmMenu *menu, const char *title);

/**
 * Get title from bmMenu instance.
 *
 * @param menu bmMenu instance where to get title from.
 * @return Pointer to null terminated C "string", can be NULL for empty title.
 */
const char* bmMenuGetTitle(const bmMenu *menu);

/**
 * Add item to bmMenu instance at specific index.
 *
 * @param menu bmMenu instance where item will be added.
 * @param item bmItem instance to add.
 * @param index Index where item will be added.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuAddItemAt(bmMenu *menu, bmItem *item, unsigned int index);

/**
 * Add item to bmMenu instance.
 *
 * @param menu bmMenu instance where item will be added.
 * @param item bmItem instance to add.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuAddItem(bmMenu *menu, bmItem *item);

/**
 * Remove item from bmMenu instance at specific index.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param index Index of item to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItemAt(bmMenu *menu, unsigned int index);

/**
 * Remove item from bmMenu instance.
 * The item won't be freed, use bmItemFree to do that.
 *
 * @param menu bmMenu instance from where item will be removed.
 * @param item bmItem instance to remove.
 * @return 1 on successful add, 0 on failure.
 */
int bmMenuRemoveItem(bmMenu *menu, bmItem *item);

/**
 * Get selected item from bmMenu instance.
 *
 * @param menu bmMenu instance from where to get selected item.
 * @return Selected bmItem instance, NULL if none selected.
 */
bmItem* bmMenuGetSelectedItem(const bmMenu *menu);

/**
 * Get items from bmMenu instance.
 *
 * @param menu bmMenu instance from where to get items.
 * @param nmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetItems(const bmMenu *menu, unsigned int *nmemb);

/**
 * Get filtered (displayed) items from bmMenu instance.
 *
 * @warning The pointer returned by this function _will_ be invalid when menu internally filters its list again.
 *          Do not store this pointer.
 *
 * @param menu bmMenu instance from where to get filtered items.
 * @param nmemb Reference to unsigned int where total count of returned items will be stored.
 * @return Pointer to array of bmItem pointers.
 */
bmItem** bmMenuGetFilteredItems(const bmMenu *menu, unsigned int *nmemb);

/**
 * Set items to bmMenu instance.
 * Will replace all the old items on bmMenu instance.
 *
 * If items is NULL, or nmemb is zero, all items will be freed from the menu.
 *
 * @param menu bmMenu instance where items will be set.
 * @param items Array of bmItem pointers to set.
 * @param nmemb Total count of items in array.
 * @return 1 on successful set, 0 on failure.
 */
int bmMenuSetItems(bmMenu *menu, const bmItem **items, unsigned int nmemb);

/**
 * Render bmMenu instance using chosen draw method.
 *
 * @param menu bmMenu instance to be rendered.
 */
void bmMenuRender(const bmMenu *menu);

/**
 * Poll key and unicode from underlying UI toolkit.
 *
 * This function will block on CURSES draw mode.
 *
 * @param menu bmMenu instance from which to poll.
 * @param unicode Reference to unsigned int.
 * @return bmKey for polled key.
 */
bmKey bmMenuGetKey(bmMenu *menu, unsigned int *unicode);

/**
 * Advances menu logic with key and unicode as input.
 *
 * @param menu bmMenu instance to be advanced.
 * @return bmRunResult for menu state.
 */
bmRunResult bmMenuRunWithKey(bmMenu *menu, bmKey key, unsigned int unicode);

/**
 * Allocate a new item.
 *
 * @param text Pointer to null terminated C "string", can be NULL for empty text.
 * @return bmItem for new item instance, NULL if creation failed.
 */
bmItem* bmItemNew(const char *text);

/**
 * Release bmItem instance.
 *
 * @param item bmItem instance to be freed from memory.
 */
void bmItemFree(bmItem *item);

/**
 * Set text to bmItem instance.
 *
 * @param item bmItem instance where to set text.
 * @param text C "string" to set as text, can be NULL for empty text.
 */
int bmItemSetText(bmItem *item, const char *text);

/**
 * Get text from bmItem instance.
 *
 * @param item bmItem instance where to get text from.
 * @return Pointer to null terminated C "string", can be NULL for empty text.
 */
const char* bmItemGetText(const bmItem *item);

/* vim: set ts=8 sw=4 tw=0 :*/
