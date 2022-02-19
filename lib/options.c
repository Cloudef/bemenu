#include "internal.h"

static struct bm_option_desc bm_options[] {
    {
        .option = BM_OPT_IGNORECASE, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'i', .long_opt = "ignorecase",
        .desc = "Match items case insensitively",
    },
    {
        .option = BM_OPT_FILTER, .type = BM_OPT_TYPE_STRING,
        .short_opt = 'F', .long_opt = "filter",
        .desc = "Filter entries for a given string",
        .need_arg = true,
    },
    {
        .option = BM_OPT_WRAP, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'w', .long_opt = "wrap",
        .desc = "Wraps cursor selection",
    },
    {
        .option = BM_OPT_LIST, .type = BM_OPT_TYPE_INT,
        .short_opt = 'l', .long_opt = "list",
        .desc = "List items vertically with the given number of lines",
        .need_arg = true,
    },
    {
        .option = BM_OPT_PROMPT, .type = BM_OPT_TYPE_STRING,
        .short_opt = 'p', .long_opt = "prompt",
        .desc = "Defines the prompt text to be displayed",
        .need_arg = true,
    },
    {
        .option = BM_OPT_PREFIX, .type = BM_OPT_TYPE_STRING,
        .short_opt = 'P', .long_opt = "prefix",
        .desc = "Text to show before highlighted item",
        .need_arg = true,
    },
    {
        .option = BM_OPT_INDEX, .type = BM_OPT_TYPE_INT,
        .short_opt = 'I', .long_opt = "index",
        .desc = "Select item at index automatically",
        .need_arg = true,
    },
    {
        .option = BM_OPT_PASSWORD, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'x', .long_opt = "password",
        .desc = "Hide input",
    },
    {
        .option = BM_OPT_NO_SPACING, .type = BM_OPT_TYPE_VOID,
        .short_opt = 's', .long_opt = "no-spacing",
        .desc = "Disable the title spacing on entries",
    },
    {
        .option = BM_OPT_SCROLLBAR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "scrollbar",
        .desc = "Display scrollbar [none (default), always, autohide]",
        .need_arg = true,
    },
    {
        .option = BM_OPT_BOTTOM, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'b', .long_opt = "bottom",
        .desc = "Appears at the bottom of the screen",
        .backends = "wx",
    },
    {
        .option = BM_OPT_CENTER, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'c', .long_opt = "center",
        .desc = "Appears at the center of the screen",
        .backends = "wx",
    },
    {
        .option = BM_OPT_NO_OVERLAP, .type = BM_OPT_TYPE_VOID,
        .short_opt = 'n', .long_opt = "no-overlap",
        .desc = "Adjust geometry to not overlap with panels",
        .backends = "w",
    },
    {
        .option = BM_OPT_MONITOR, .type = BM_OPT_TYPE_STRING_OR_INT,
        .short_opt = 'm', .long_opt = "monitor",
        .desc = "Index or name of the monitor where menu will appear [-1 (focused), -2 (all)]",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_LINE_HEIGHT, .type = BM_OPT_TYPE_INT,
        .short_opt = 'H', .long_opt = "line-height",
        .desc = "Defines the height to make each menu line [0 (default height)]",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_MARGIN, .type = BM_OPT_TYPE_INT,
        .short_opt = 'M', .long_opt = "margin",
        .desc = "Defines the empty space on either side of the menu",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_WIDTH_FACTOR, .type = BM_OPT_TYPE_FLOAT,
        .short_opt = 'W', .long_opt = "width-factor",
        .desc = "Defines the relative width factor of the menu [0..1]",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_CURSOR_HEIGHT, .type = BM_OPT_TYPE_INT,
        .short_opt = 0, .long_opt = "ch",
        .desc = "Defines the height of the cursor [0 (scales with line height)]",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_FONT, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "fn",
        .desc = "Defines the font to be used [name size?]",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_TITLE_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "tb",
        .desc = "Defines the title background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_TITLE_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "tf",
        .desc = "Defines the title foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_FILTER_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "fb",
        .desc = "Defines the filter background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_FILTER_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "ff",
        .desc = "Defines the filter foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_NORMAL_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "nb",
        .desc = "Defines the normal background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_NORMAL_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "nf",
        .desc = "Defines the normal foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_HIGHLIGHT_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "hb",
        .desc = "Defines the highlighted background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_HIGHLIGHT_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "hf",
        .desc = "Defines the highlighted foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_FEEDBACK_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "fbb",
        .desc = "Defines the feedback background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_FEEDBACK_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "fbf",
        .desc = "Defines the feedback foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_SELECTED_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "sb",
        .desc = "Defines the selected background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_SELECTED_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "sf",
        .desc = "Defines the selected foreground color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_SCROLLBAR_BG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "scb",
        .desc = "Defines the scrollbar background color",
        .backends = "wx",
        .need_arg = true,
    },
    {
        .option = BM_OPT_SCROLLBAR_FG_COLOR, .type = BM_OPT_TYPE_STRING,
        .short_opt = 0, .long_opt = "scf",
        .desc = "Defines the scrollbar foreground color",
        .backends = "wx",
        .need_arg = true,
    },
}

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

static void
bm_parse_monitor_opt(struct bm_menu *menu, const char *arg)
{
    char *endptr = NULL;
    const long num = strtol(arg, &endptr, 10);
    if (arg == endptr) { // No digits found
        if (!strcmp(arg, "focused")) {
            bm_menu_set_monitor(menu, -1);
        } else if (!strcmp(arg, "all")) {
            bm_menu_set_monitor(menu, -2);
        } else {
            bm_menu_set_monitor_name(menu, arg);
        }
    } else {
        bm_menu_set_monitor(menu, num);
    }
}

static void
bm_menu_apply_option(struct bm_menu *menu, enum bm_option option, const char *optarg)
{
    assert(menu);

    const struct {
        enum bm_option opt;
        enum bm_color col;
    } map[BM_COLOR_LAST] = {
        { BM_OPT_TITLE_BG_COLOR, BM_COLOR_TITLE_BG },
        { BM_OPT_TITLE_FG_COLOR, BM_COLOR_TITLE_FG },
        { BM_OPT_FILTER_BG_COLOR, BM_COLOR_FILTER_BG },
        { BM_OPT_FILTER_FG_COLOR, BM_COLOR_FILTER_FG },
        { BM_OPT_NORMAL_BG_COLOR, BM_COLOR_ITEM_BG },
        { BM_OPT_NORMAL_FG_COLOR, BM_COLOR_ITEM_FG },
        { BM_OPT_HIGHLIGHTED_BG_COLOR, BM_COLOR_HIGHLIGHTED_BG },
        { BM_OPT_HIGHLIGHTED_FG_COLOR, BM_COLOR_HIGHLIGHTED_FG },
        { BM_OPT_FEEDBACK_BG_COLOR, BM_COLOR_FEEDBACK_BG },
        { BM_OPT_FEEDBACK_FG_COLOR, BM_COLOR_FEEDBACK_FG },
        { BM_OPT_SELECTED_BG_COLOR, BM_COLOR_SELECTED_BG },
        { BM_OPT_SELECTED_FG_COLOR, BM_COLOR_SELECTED_FG },
        { BM_OPT_SCROLLBAR_BG_COLOR, BM_COLOR_SCROLLBAR_BG },
        { BM_OPT_SCROLLBAR_FG_COLOR, BM_COLOR_SCROLLBAR_FG },
    };

    for (size_t i = 0; i < ARRAY_LENGTH(map); ++i) {
        if (map[i].opt != option) continue;
        bm_menu_set_color(menu, map[i].col, optarg);
        return;
    }

    switch (option) {
        case BM_OPT_IGNORE_CASE:
            bm_menu_set_filter_mode(menu, BM_FILTER_MODE_DMENU_CASE_INSENSITIVE);
            break;
        case BM_OPT_FILTER:
            bm_menu_set_filter(menu, optarg);
            break;
        case BM_OPT_WRAP:
            bm_menu_set_wrap(menu, true);
            break;
        case BM_OPT_LIST:
            bm_menu_set_lines(menu, strtol(optarg, NULL, 10));
            break;
        case BM_OPT_CENTER:
            bm_menu_set_align(menu, BM_ALIGN_CENTER);
            break;
        case BM_OPT_PROMPT:
            bm_menu_set_title(menu, optarg);
            break;
        case BM_OPT_PREFIX:
            bm_menu_set_prefix(menu, optarg);
            break;
        case BM_OPT_INDEX:
            bm_menu_set_highlighted_index(menu, strtol(optarg, NULL, 10));
            break;
        case BM_OPT_SCROLLBAR:
            bm_menu_set_scrollbar(menu,
                    (!strcmp(optarg, "none") ? BM_SCROLLBAR_NONE :
                     (!strcmp(optarg, "always") ? BM_SCROLLBAR_ALWAYS :
                      (!strcmp(optarg, "autohide") ? BM_SCROLLBAR_AUTOHIDE :
                       BM_SCROLLBAR_NONE))));
            break;
        case BM_OPT_PASSWORD:
            bm_menu_set_password(menu, true);
            break;
        case BM_OPT_BOTTOM:
            bm_menu_set_align(menu, BM_ALIGN_BOTTOM);
            break;
        case BM_OPT_MONITOR:
            bm_parse_monitor_opt(menu, optarg);
            break;
        case BM_OPT_NO_OVERLAP:
            bm_menu_set_panel_overlap(menu, false);
            break;
        case BM_OPT_SPACING:
            bm_menu_set_spacing(menu, false);
            break;
        case BM_OPT_LINE_HEIGHT:
            bm_menu_set_line_height(menu, strtol(optarg, NULL, 10));
            break;
        case BM_OPT_HEIGHT_MARGIN:
            bm_menu_set_width(menu, strtol(optarg, NULL, 10), bm_menu_get_width_factor(menu));
            break;
        case BM_OPT_WIDTH_FACTOR:
            bm_menu_set_width(menu, bm_menu_get_hmargin_size(menu),  strtof(optarg, NULL));
            break;
        case BM_OPT_CURSOR_HEIGHT:
            bm_menu_set_cursor_height(menu, strtol(optarg, NULL, 10));
            break;
        case BM_OPT_FONT:
            bm_menu_set_font(menu, optarg);
            break;
        case BM_OPT_TITLE_COLOR_BG:
            bm_menu_set_color(menu, BM_COLOR_TITLE_BG, optarg);
            break;
    }
}

bool
bm_menu_parse_options(struct bm_menu *menu, int argc, const char *argv[]);
{
    {
        int num_opts;
        char **opts;
        const char *env;
        if ((env = getenv("BEMENU_OPTS")) && (opts = tokenize_quoted_to_argv(env, argv, &num_opts)))
            bm_parse(menu, &num_opts, &opts);
    }
    bm_parse(menu, argc, argv);
};

const struct bm_option_desc*
bm_available_options(size_t *out_num)
{
    assert(out_num);
    *out_num = ARRAY_LENGTH(bm_options);
    return bm_options;
}
