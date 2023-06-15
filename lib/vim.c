#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

#include "vim.h"


static enum bm_vim_code vim_on_first_key(struct bm_menu *menu, uint32_t unicode, uint32_t item_count, uint32_t items_displayed);
static enum bm_vim_code vim_on_second_key(struct bm_menu *menu, uint32_t unicode, uint32_t item_count, uint32_t items_displayed);


static void move_left(struct bm_menu *menu){
    if(menu->filter && menu->cursor > 0){
        uint32_t old_cursor = menu->cursor;
        menu->cursor -= bm_utf8_rune_prev(menu->filter, menu->cursor);
        menu->curses_cursor -= bm_utf8_rune_width(menu->filter + menu->cursor, old_cursor - menu->cursor);
    }
}

static void move_right(struct bm_menu *menu, size_t filter_length){
    if(menu->cursor < filter_length){
        uint32_t old_cursor = menu->cursor;
        menu->cursor += bm_utf8_rune_next(menu->filter, menu->cursor);
        menu->curses_cursor += bm_utf8_rune_width(menu->filter + old_cursor, menu->cursor - old_cursor);
    }
}

static void move_line_start(struct bm_menu *menu){
    menu->cursor = 0;
    menu->curses_cursor = 0;
}

static void move_line_end(struct bm_menu *menu){
    menu->cursor = (menu->filter ? strlen(menu->filter) : 0);
    menu->curses_cursor = (menu->filter ? bm_utf8_string_screen_width(menu->filter) : 0);
}

static void menu_next(struct bm_menu *menu, uint32_t count, bool wrap){
    if (menu->index < count - 1) {
        bm_menu_set_highlighted_index(menu, menu->index + 1);
    } else if (wrap) {
        bm_menu_set_highlighted_index(menu, 0);
    }
}

static void menu_prev(struct bm_menu *menu, uint32_t count, bool wrap){
    if (menu->index > 0) {
        bm_menu_set_highlighted_index(menu, menu->index - 1);
    } else if (wrap) {
        bm_menu_set_highlighted_index(menu, count - 1);
    }
}

static void menu_first(struct bm_menu *menu, uint32_t count){
    if(count > 0){
        bm_menu_set_highlighted_index(menu, 0);
    }
}

static void menu_last(struct bm_menu *menu, uint32_t count){
    if(count > 0){
        bm_menu_set_highlighted_index(menu, count - 1);
    }
}

static void menu_view_high(struct bm_menu *menu, uint32_t count){
    if(count > 0){
        uint32_t nth_view_item = menu->index % menu->lines;
        bm_menu_set_highlighted_index(menu, menu->index - nth_view_item);
    }
}

static void menu_view_mid(struct bm_menu *menu, uint32_t count, uint32_t displayed){
    if(count > 0){
        uint32_t nth_view_item = menu->index % menu->lines;
        uint32_t half_view_height = (displayed - 1) / 2;
        bm_menu_set_highlighted_index(menu, menu->index - nth_view_item + half_view_height);
    }
}

static void menu_view_low(struct bm_menu *menu, uint32_t count, uint32_t displayed){
    if(count > 0){
        uint32_t nth_view_item = menu->index % menu->lines;
        uint32_t view_bottom_offset = (displayed - 2) - nth_view_item;
        bm_menu_set_highlighted_index(menu, menu->index + view_bottom_offset);
    }
}

static void menu_page_down(struct bm_menu *menu, uint32_t count, uint32_t displayed){
    bm_menu_set_highlighted_index(menu, (menu->index + displayed >= count ? count - 1 : menu->index + (displayed - 1)));
}

static void menu_page_up(struct bm_menu *menu, uint32_t displayed){
    bm_menu_set_highlighted_index(menu, (menu->index < displayed ? 0 : menu->index - (displayed - 1)));
}

static void move_word(struct bm_menu *menu){
    if(!menu->filter) return;

    size_t filter_length = strlen(menu->filter);
    while (menu->cursor < filter_length && !isspace(menu->filter[menu->cursor])) move_right(menu, filter_length);
    while (menu->cursor < filter_length && isspace(menu->filter[menu->cursor])) move_right(menu, filter_length);
}

static void move_word_back(struct bm_menu *menu){
    while (menu->cursor > 0 && isspace(menu->filter[menu->cursor - 1])) move_left(menu);
    while (menu->cursor > 0 && !isspace(menu->filter[menu->cursor - 1])) move_left(menu);
}

static void move_word_end(struct bm_menu *menu){
    if(!menu->filter) return;

    size_t filter_length = strlen(menu->filter);
    size_t old_cursor = menu->cursor;
    size_t old_curses_cursor = menu->curses_cursor;

    while (menu->cursor < filter_length && isspace(menu->filter[menu->cursor + 1])) move_right(menu, filter_length);
    while (menu->cursor < filter_length && !isspace(menu->filter[menu->cursor + 1])) move_right(menu, filter_length);

    if(menu->cursor == filter_length){
        menu->cursor = old_cursor;
        menu->curses_cursor = old_curses_cursor;
    }
}

static void delete_char(struct bm_menu *menu){
    if(menu->filter){
        if(menu->cursor < strlen(menu->filter)){
            size_t width;
            bm_utf8_rune_remove(menu->filter, menu->cursor + 1, &width);
        }
    }
}

static void delete_char_back(struct bm_menu *menu){
    if(menu->filter){
        if(menu->cursor > 0){
            size_t width;
            bm_utf8_rune_remove(menu->filter, menu->cursor, &width);
            move_left(menu);
        }
    }
}

static void delete_word(struct bm_menu *menu){
    if(!menu->filter) return;

    size_t filter_length = strlen(menu->filter);
    while (menu->cursor < filter_length && !isspace(menu->filter[menu->cursor])) delete_char(menu);
    while (menu->cursor < filter_length && isspace(menu->filter[menu->cursor])) delete_char(menu);
}

static void delete_word_back(struct bm_menu *menu){
    while (menu->cursor > 0 && isspace(menu->filter[menu->cursor - 1])){
        move_left(menu);
        delete_char(menu);
    }

    while (menu->cursor > 0 && !isspace(menu->filter[menu->cursor - 1])){
        move_left(menu);
        delete_char(menu);
    } 
}

static void delete_line(struct bm_menu *menu){
    if(menu->filter){
        menu->filter[0] = 0;
        menu->cursor = 0;
        menu->curses_cursor = 0;
    }
}

static void delete_to_line_end(struct bm_menu *menu){
    if(menu->filter){
        menu->filter[menu->cursor] = 0;
    }
}

static void delete_to_line_start(struct bm_menu *menu){
    if(menu->filter){
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wrestrict"
        strcpy(menu->filter, menu->filter + menu->cursor);
#pragma GCC diagnostic pop

        menu->cursor = 0;
        menu->curses_cursor = 0;
    }
}

static void toggle_item_selection(struct bm_menu *menu){
    struct bm_item *highlighted = bm_menu_get_highlighted_item(menu);
    if (highlighted){
        if(!bm_menu_item_is_selected(menu, highlighted)){
            list_add_item(&menu->selection, highlighted);
        } else {
            list_remove_item(&menu->selection, highlighted);
        }
    }
}

enum bm_vim_code bm_vim_key_press(struct bm_menu *menu, enum bm_key key, uint32_t unicode, uint32_t item_count, uint32_t items_displayed){
    if(key == BM_KEY_ESCAPE && unicode == 99) return BM_VIM_EXIT;

    if(menu->vim_mode == 'n'){
        if(key == BM_KEY_ESCAPE){
            if(menu->vim_esc_exits) return BM_VIM_EXIT;
            return BM_VIM_CONSUME;
        }

        if(unicode == 0 || unicode > 128) return BM_VIM_IGNORE;

        if(menu->vim_last_key == 0) return vim_on_first_key(menu, unicode, item_count, items_displayed);
        else return vim_on_second_key(menu, unicode, item_count, items_displayed);
    } else if(menu->vim_mode == 'i'){
        if(key == BM_KEY_ESCAPE && (unicode == 0 || unicode == 27)){
            menu->vim_mode = 'n';
            return BM_VIM_CONSUME;
        }
    }

    return BM_VIM_IGNORE;
}

static enum bm_vim_code vim_on_first_key(struct bm_menu *menu, uint32_t unicode, uint32_t item_count, uint32_t items_displayed){
    size_t filter_length = 0;

    switch(unicode){
        case 'q':
            return BM_VIM_EXIT;
        case 'v':
            toggle_item_selection(menu);
            goto action_executed;
        case 'i':
            goto insert_action_executed;
        case 'I':
            move_line_start(menu);
            goto insert_action_executed;
        case 'a':
            if(menu->filter) filter_length = strlen(menu->filter);
            move_right(menu, filter_length);
            goto insert_action_executed;
        case 'A':
            move_line_end(menu);
            goto insert_action_executed;
        case 'h':
            move_left(menu);
            goto action_executed;
        case 'n':
        case 'j':
            menu_next(menu, item_count, menu->wrap);
            goto action_executed;
        case 'p':
        case 'k':
            menu_prev(menu, item_count, menu->wrap);
            goto action_executed;
        case 'l':
            if(menu->filter) filter_length = strlen(menu->filter);
            move_right(menu, filter_length);
            goto action_executed;
        case 'w':
            move_word(menu);
            goto action_executed;
        case 'b':
            move_word_back(menu);
            goto action_executed;
        case 'e':
            move_word_end(menu);
            goto action_executed;
        case 'x':
            delete_char(menu);
            goto action_executed;
        case 'X':
            delete_char_back(menu);
            goto action_executed;
        case '0':
            move_line_start(menu);
            goto action_executed;
        case '$':
            move_line_end(menu);
            goto action_executed;
        case 'G':
            menu_last(menu, item_count);
            goto action_executed;
        case 'H':
            menu_view_high(menu, item_count);
            goto action_executed;
        case 'M':
            menu_view_mid(menu, item_count, items_displayed);
            goto action_executed;
        case 'L':
            menu_view_low(menu, item_count, items_displayed);
            goto action_executed;
        case 'F':
            menu_page_down(menu, item_count, items_displayed);
            goto action_executed;
        case 'B':
            menu_page_up(menu, items_displayed);
            goto action_executed;
        case 'c':
        case 'd':
        case 'g':
            menu->vim_last_key = unicode;
            return BM_VIM_CONSUME;
        default:
            menu->vim_last_key = 0;
            return BM_VIM_CONSUME;
    }

insert_action_executed:
    menu->vim_mode = 'i';
action_executed:
    menu->vim_last_key = 0;
    return BM_VIM_CONSUME;
}

static enum bm_vim_code vim_on_second_key(struct bm_menu *menu, uint32_t unicode, uint32_t item_count, uint32_t items_displayed){
    if(menu->vim_last_key == 'd'){
        switch(unicode){
            case 'd':
                delete_line(menu);
                goto action_executed;
            case 'w':
                delete_word(menu);
                goto action_executed;
            case 'b':
                delete_word_back(menu);
                goto action_executed;
            case '$':
                delete_to_line_end(menu);
                goto action_executed;
            case '0':
                delete_to_line_start(menu);
                goto action_executed;
        }
    } else if(menu->vim_last_key == 'c'){
        switch(unicode){
            case 'c':
                delete_line(menu);
                goto insert_action_executed;
            case 'w':
                delete_word(menu);
                goto insert_action_executed;
            case 'b':
                delete_word_back(menu);
                goto insert_action_executed;
            case '$':
                delete_to_line_end(menu);
                goto insert_action_executed;
            case '0':
                delete_to_line_start(menu);
                goto insert_action_executed;
        }
    } else if(menu->vim_last_key == 'g'){
        switch(unicode){
            case 'g':
                menu_first(menu, item_count);
                goto action_executed;
        }
    }

    return vim_on_first_key(menu, unicode, item_count, items_displayed);

insert_action_executed:
    menu->vim_mode = 'i';
action_executed:
    menu->vim_last_key = 0;
    return BM_VIM_CONSUME;
}
