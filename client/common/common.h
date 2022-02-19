#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_

#include <bemenu.h>
#include <stddef.h>

struct client {
    bool grab;
    bool ifne;
    bool force_fork, fork;
    bool no_exec;
};

void parse_args(struct client *client, int *argc, char **argv[]);
struct bm_menu* menu_with_options(struct client *client);
enum bm_run_result run_menu(const struct client *client, struct bm_menu *menu, void (*item_cb)(const struct client *client, struct bm_item *item));

#endif /* _BM_COMMON_H_ */

/* vim: set ts=8 sw=4 tw=0 :*/
