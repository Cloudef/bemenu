#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_

#include <bemenu.h>

struct client {
    enum bm_filter_mode filter_mode;
    enum bm_scrollbar_mode scrollbar;
    const char *colors[BM_COLOR_LAST];
    const char *title;
    const char *prefix;
    const char *font;
    uint32_t lines;
    uint32_t selected;
    uint32_t monitor;
    bool bottom;
    bool grab;
    bool wrap;
};

void parse_args(struct client *client, int *argc, char **argv[]);
struct bm_menu* menu_with_options(const struct client *client);
enum bm_run_result run_menu(const struct client *client, struct bm_menu *menu, void (*item_cb)(struct bm_item *item, const char *text));

#endif /* _BM_COMMON_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
