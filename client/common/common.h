#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_

#include <bemenu.h>
#include <stddef.h>

struct client {
    enum bm_filter_mode filter_mode;
    enum bm_scrollbar_mode scrollbar;
    const char *colors[BM_COLOR_LAST];
    const char *title;
    const char *prefix;
    const char *font;
    const char *initial_filter;
    uint32_t line_height;
    uint32_t cursor_height;
    uint32_t lines;
    uint32_t selected;
    uint32_t monitor;
    bool bottom;
    bool center;
    bool grab;
    bool wrap;
    bool ifne;
    bool no_overlap;
    bool no_spacing;
    bool force_fork, fork;
    bool no_exec;
    bool password;
    char *monitor_name;
};

char* cstrcopy(const char *str, size_t size);
char** tokenize_quoted_to_argv(const char *str, char *argv0, int *out_argc);
void parse_args(struct client *client, int *argc, char **argv[]);
struct bm_menu* menu_with_options(struct client *client);
enum bm_run_result run_menu(const struct client *client, struct bm_menu *menu, void (*item_cb)(const struct client *client, struct bm_item *item));

#endif /* _BM_COMMON_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
