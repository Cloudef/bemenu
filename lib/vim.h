#ifndef _VIM_H_
#define _VIM_H_

#include "internal.h"

enum bm_vim_code {
    BM_VIM_CONSUME,
    BM_VIM_IGNORE,
    BM_VIM_EXIT
};

enum bm_vim_code bm_vim_key_press(struct bm_menu *menu, enum bm_key key, uint32_t unicode, uint32_t item_count, uint32_t items_displayed);

#endif
