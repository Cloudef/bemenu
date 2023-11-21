#include "../client/common/common.h"

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
 * Default title/prompt for the bmenu client (Can be changed with `-p` option).
 */
static struct client default_bmenu_client = {
        .filter_mode = BM_FILTER_MODE_DMENU,
        .title = "bemenu",
        .monitor = -1,
};

/**
 * Default title/prompt for the bmenu-run client (Can be changed with `-p` option).
 */
static struct client default_bmenu_run_client = {
        .filter_mode = BM_FILTER_MODE_DMENU,
        .title = "bemenu-run",
        .monitor = -1,
};
