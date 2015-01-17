#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_

#include <bemenu.h>

struct client {
    enum bm_prioritory prioritory;
    enum bm_filter_mode filter_mode;
    int32_t wrap;
    uint32_t lines;
    const char *colors[BM_COLOR_LAST];
    const char *title;
    const char *prefix;
    const char *renderer;
    const char *font;
    int32_t selected;
    int32_t bottom;
    int32_t grab;
    int32_t monitor;
};

void parse_args(struct client *client, int *argc, char **argv[]);
struct bm_menu* menu_with_options(struct client *client);
enum bm_run_result run_menu(struct bm_menu *menu);

#endif /* _BM_COMMON_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
