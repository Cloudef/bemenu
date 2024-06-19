#ifndef _BM_CAIRO_H_
#define _BM_CAIRO_H_

#include "internal.h"
#include <string.h>
#include <assert.h>
#include <math.h>
#include <cairo.h>
#include <pango/pangocairo.h>

struct cairo {
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoContext *pango;
    double scale;
};

struct cairo_color {
    float r, g, b, a;
};

struct cairo_paint {
    struct cairo_color fg;
    struct cairo_color bg;
    const char *font;
    int32_t baseline;
    uint32_t cursor;
    uint32_t cursor_height;
    uint32_t cursor_width;
    struct cairo_color cursor_fg;
    struct cairo_color cursor_bg;
    uint32_t hpadding;
    bool draw_cursor;

    struct box {
        int32_t lx, rx; // left/right offset (pos.x - lx, box.w + rx)
        int32_t ty, by; // top/bottom offset (pos.y - ty, box.h + by)
        int32_t w, h; // 0 for text width/height
    } box;

    struct pos {
        int32_t x, y;
    } pos;
};

struct cairo_result {
    uint32_t x_advance;
    uint32_t height;
    uint32_t baseline;
};

struct cairo_paint_result {
    uint32_t displayed;
    uint32_t height;
};

static size_t blen = 0;
static char *buffer = NULL;

static inline bool
bm_cairo_create_for_surface(struct cairo *cairo, cairo_surface_t *surface)
{
    assert(cairo && surface);
    if (!(cairo->cr = cairo_create(surface)))
        goto fail;

    if (!(cairo->pango = pango_cairo_create_context(cairo->cr)))
        goto fail;

    cairo_set_antialias(cairo->cr, CAIRO_ANTIALIAS_DEFAULT);

    cairo->surface = surface;
    assert(cairo->scale > 0);
    cairo_surface_set_device_scale(surface, cairo->scale, cairo->scale);
    return true;

fail:
    if (cairo->cr)
      cairo_destroy(cairo->cr);
    return false;
}

static inline void
bm_cairo_destroy(struct cairo *cairo)
{
    if (cairo->cr)
        cairo_destroy(cairo->cr);
    if (cairo->surface)
        cairo_surface_destroy(cairo->surface);
}

static inline PangoLayout*
bm_pango_get_layout(struct cairo *cairo, struct cairo_paint *paint, const char *buffer)
{
    PangoLayout *layout = pango_cairo_create_layout(cairo->cr);
    pango_layout_set_text(layout, buffer, -1);
    PangoFontDescription *desc = pango_font_description_from_string(paint->font);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_single_paragraph_mode(layout, 1);
    pango_font_description_free(desc);
    return layout;
}

BM_LOG_ATTR(4, 5) static inline bool
bm_pango_get_text_extents(struct cairo *cairo, struct cairo_paint *paint, struct cairo_result *result, const char *fmt, ...)
{
    assert(cairo && paint && result && fmt);
    memset(result, 0, sizeof(struct cairo_result));

    va_list args;
    va_start(args, fmt);
    bool ret = bm_vrprintf(&buffer, &blen, fmt, args);
    va_end(args);

    if (!ret)
        return false;

    PangoRectangle rect;
    PangoLayout *layout = bm_pango_get_layout(cairo, paint, buffer);
    pango_layout_get_pixel_extents(layout, NULL, &rect);
    int baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;
    g_object_unref(layout);

    result->x_advance = rect.x + rect.width;
    result->height = rect.height;
    result->baseline = baseline;
    return true;
}

static inline bool
bm_cairo_draw_line_str(struct cairo *cairo, struct cairo_paint *paint, struct cairo_result *result, const char *buffer)
{
    cairo_save(cairo->cr);
    PangoLayout *layout = bm_pango_get_layout(cairo, paint, buffer);
    pango_cairo_update_layout(cairo->cr, layout);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);
    height = paint->box.h > 0 ? paint->box.h : height;
    int base = pango_layout_get_baseline(layout) / PANGO_SCALE;

    uint32_t line_height = height + paint->box.by + paint->box.ty;
    cairo_set_source_rgba(cairo->cr, paint->bg.r, paint->bg.b, paint->bg.g, paint->bg.a);
    cairo_rectangle(cairo->cr,
            paint->pos.x - paint->box.lx, paint->pos.y - paint->box.ty,
            (paint->box.w > 0 ? paint->box.w : width) + paint->box.rx + paint->box.lx,
            line_height);
    cairo_fill(cairo->cr);

    cairo_set_source_rgba(cairo->cr, paint->fg.r, paint->fg.b, paint->fg.g, paint->fg.a);
    cairo_move_to(cairo->cr, paint->box.lx + paint->pos.x, paint->pos.y - base + paint->baseline);
    pango_cairo_show_layout(cairo->cr, layout);

    if (paint->draw_cursor) {
        PangoRectangle rect;
        pango_layout_index_to_pos(layout, paint->cursor, &rect);

        if (!rect.width) {
            struct cairo_result result = {0};
            bm_pango_get_text_extents(cairo, paint, &result, "#");
            rect.width = result.x_advance * PANGO_SCALE;
        }

        uint32_t cursor_width = rect.width / PANGO_SCALE;
        if (paint->cursor_width > 0) {
            cursor_width = paint->cursor_width;
        }
        uint32_t cursor_height = fmin(paint->cursor_height == 0 ? line_height : paint->cursor_height, line_height);
        cairo_set_source_rgba(cairo->cr, paint->cursor_fg.r, paint->cursor_fg.b, paint->cursor_fg.g, paint->cursor_fg.a);
        cairo_rectangle(cairo->cr,
                paint->pos.x + paint->box.lx + rect.x / PANGO_SCALE, paint->pos.y - paint->box.ty + ((line_height - cursor_height) / 2),
                cursor_width, cursor_height);
        cairo_fill(cairo->cr);

        cairo_rectangle(cairo->cr,
                paint->pos.x + paint->box.lx + rect.x / PANGO_SCALE, paint->pos.y - paint->box.ty,
                cursor_width, line_height);
        cairo_clip(cairo->cr);

        cairo_set_source_rgba(cairo->cr, paint->cursor_bg.r, paint->cursor_bg.b, paint->cursor_bg.g, paint->cursor_bg.a);
        cairo_move_to(cairo->cr, paint->box.lx + paint->pos.x, paint->pos.y - base + paint->baseline);
        pango_cairo_show_layout(cairo->cr, layout);
        cairo_reset_clip(cairo->cr);
    }

    g_object_unref(layout);

    result->x_advance = width + paint->box.rx;
    result->height = line_height;

    cairo_identity_matrix(cairo->cr);
    cairo_restore(cairo->cr);
    return true;
}

BM_LOG_ATTR(4, 5) static inline bool
bm_cairo_draw_line(struct cairo *cairo, struct cairo_paint *paint, struct cairo_result *result, const char *fmt, ...)
{
    assert(cairo && paint && result && fmt);
    memset(result, 0, sizeof(struct cairo_result));

    va_list args;
    va_start(args, fmt);
    bool ret = bm_vrprintf(&buffer, &blen, fmt, args);
    va_end(args);

    if (!ret)
        return false;

    return bm_cairo_draw_line_str(cairo, paint, result, buffer);
}

static inline void
bm_cairo_color_from_menu_color(const struct bm_menu *menu, enum bm_color color, struct cairo_color *c)
{
    assert(menu);
    c->r = (float)menu->colors[color].r / 255.0f;
    c->g = (float)menu->colors[color].g / 255.0f;
    c->b = (float)menu->colors[color].b / 255.0f;
    c->a = (float)menu->colors[color].a / 255.0f;
}

static char *
bm_cairo_entry_message(char *entry_text, bool highlighted, uint32_t event_feedback, uint32_t index, uint32_t count)
{
    if (!highlighted || !event_feedback) {
        return entry_text ? entry_text : "";
    } else {
        if (event_feedback & TOUCH_WILL_CANCEL) {
            return "Cancel…";
        }
        if (event_feedback & TOUCH_WILL_SCROLL_FIRST) {
            if (index == 0) {
                return "Already on the first page…";
            } else {
                return "First page…";
            }
        }
        if (event_feedback & TOUCH_WILL_SCROLL_UP) {
            if (index == 0) {
                return "Already on the first page…";
            } else {
                return "Previous page…";
            }
        }
        if (event_feedback & TOUCH_WILL_SCROLL_DOWN) {
            if (index == count - 1) {
                return "Already on the last page…";
            } else {
                return "Next page…";
            }
        }
        if (event_feedback & TOUCH_WILL_SCROLL_LAST) {
            if (index == count - 1) {
                return "Already on the last page…";
            } else {
                return "Last page…";
            }
        }
        return "Not handled feedback…";
    }
}

static inline void
bm_cairo_rounded_path(cairo_t *cr, double x, double y, double width, double height, double radius)
{
    double adjusted_radius = MIN(MIN(radius, height * 0.5), width * 0.5); /* Prevent border from intersecting itself. */
    double degrees = M_PI / 180;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - adjusted_radius, y + adjusted_radius, adjusted_radius, -90 * degrees, 0 * degrees);
    cairo_arc(cr, x + width - adjusted_radius, y + height - adjusted_radius, adjusted_radius, 0 * degrees, 90 * degrees);
    cairo_arc(cr, x + adjusted_radius, y + height - adjusted_radius, adjusted_radius, 90 * degrees, 180 * degrees);
    cairo_arc(cr, x + adjusted_radius, y + adjusted_radius, adjusted_radius, 180 * degrees, 270 * degrees);
    cairo_close_path(cr);
}


static inline void
bm_cairo_paint(struct cairo *cairo, uint32_t width, uint32_t max_height, struct bm_menu *menu, struct cairo_paint_result *out_result)
{
    assert(cairo && menu && out_result);

    max_height /= cairo->scale;

    memset(out_result, 0, sizeof(struct cairo_paint_result));
    out_result->displayed = 1;

    struct cairo_paint paint = {0};
    paint.font = menu->font.name;

    struct cairo_result result = {0};
    int ascii_height;
    bm_pango_get_text_extents(cairo, &paint, &result, "!\"#$%%&'()*+,-./0123456789:;<=>?@ABCD"
                              "EFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
    ascii_height = result.height;
    paint.baseline = result.baseline;

    double border_size = menu->border_size;
    width -= border_size;
    uint32_t height = fmin(fmax(menu->line_height, ascii_height), max_height);
    uint32_t vpadding = (height - ascii_height)/2;

    double border_radius = menu->border_radius;

    uint32_t total_item_count = menu->items.count;
    uint32_t filtered_item_count;
    bm_menu_get_filtered_items(menu, &filtered_item_count);

    cairo_save(cairo->cr);
    cairo_set_operator(cairo->cr, CAIRO_OPERATOR_CLEAR);
    cairo_reset_clip(cairo->cr);
    cairo_paint(cairo->cr);
    cairo_restore(cairo->cr);

    uint32_t count;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    uint32_t lines = (menu->lines > 0 ? menu->lines : 1);
    uint32_t page_length = 0;

    if (menu->lines_mode == BM_LINES_UP && !menu->fixed_height) {
        int32_t new_y_offset = (count < lines ? (lines - count) * height : 0);
        bm_menu_set_y_offset(menu, new_y_offset);
    }

    memset(&result, 0, sizeof(result));
    uint32_t title_x = 0;

    uint32_t total_height=menu->lines*height; /*Total height of the full menu*/
    if (!menu->fixed_height){
        total_height = MIN(count*height, total_height);
    }
    total_height += height+border_size*2;
    total_height = MIN(total_height, (max_height/height)*height);

    bm_cairo_rounded_path(cairo->cr, 1, 1, ((width + border_size)/cairo->scale)-2, total_height-2, border_radius);
    cairo_clip(cairo->cr);

    if (menu->title) {
        /* Essentially hide the title text if we are drawing lines "up". */
        enum bm_color title_fg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_TITLE_FG);
        enum bm_color title_bg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_TITLE_BG);
        bm_cairo_color_from_menu_color(menu, title_fg, &paint.fg);
        bm_cairo_color_from_menu_color(menu, title_bg, &paint.bg);
        paint.pos = (struct pos){ border_size + 4, vpadding + border_size };
        paint.box = (struct box){ 4, 16, vpadding, -vpadding, 0, height };
        bm_cairo_draw_line(cairo, &paint, &result, "%s", menu->title);
        title_x = result.x_advance;
    }

    enum bm_color filter_fg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_FILTER_FG);
    enum bm_color filter_bg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_FILTER_BG);
    enum bm_color cursor_fg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_CURSOR_FG);
    enum bm_color cursor_bg = (menu->lines_mode == BM_LINES_UP ? BM_COLOR_ITEM_BG : BM_COLOR_CURSOR_BG);
    bm_cairo_color_from_menu_color(menu, filter_fg, &paint.fg);
    bm_cairo_color_from_menu_color(menu, filter_bg, &paint.bg);
    bm_cairo_color_from_menu_color(menu, cursor_fg, &paint.cursor_fg);
    bm_cairo_color_from_menu_color(menu, cursor_bg, &paint.cursor_bg);
    paint.draw_cursor = true;
    paint.cursor = menu->cursor;
    paint.cursor_height = menu->cursor_height;
    paint.cursor_width = menu->cursor_width;
    paint.pos = (struct pos){ (menu->title ? 6 : 0) + result.x_advance + border_size, vpadding + border_size };
    paint.box = (struct box){ (menu->title ? 2 : 4), 0, vpadding, -vpadding, width - paint.pos.x, height };

    if (menu->lines == 0 || menu->lines_mode == BM_LINES_DOWN) {
        const char *filter_text = (menu->filter ? menu->filter : "");
        if (menu->password == BM_PASSWORD_HIDE) {
            bm_cairo_draw_line_str(cairo, &paint, &result, "");
        } else if (menu->password == BM_PASSWORD_INDICATOR) {
            char asterisk_print[1024] = "";
        
            for (int i = 0; i < (int)(strlen(filter_text)); ++i) asterisk_print[i] = '*';
        
            bm_cairo_draw_line(cairo, &paint, &result, "%s", asterisk_print);
        } else if (menu->password == BM_PASSWORD_NONE) {
            bm_cairo_draw_line(cairo, &paint, &result, "%s", filter_text);
        }
    }

    paint.draw_cursor = false;
    uint32_t titleh = (result.height > 0 ? result.height : height);
    out_result->height = titleh;

    uint32_t posy = (menu->lines_mode == BM_LINES_UP ? 0 : titleh);
    uint32_t spacing_x = menu->spacing ? title_x : 0, spacing_y = 0; // 0 == variable width spacing

    if (menu->lines > 0) {
        /* vertical mode */

        const bool scrollbar = (menu->scrollbar > BM_SCROLLBAR_NONE && (menu->scrollbar != BM_SCROLLBAR_AUTOHIDE || count > lines) ? true : false);
        if (lines > max_height / titleh) {
            /* there is more lines than screen can fit, enter fixed spacing mode */
            lines = max_height / titleh - 1;
            spacing_y = titleh;
        }

        uint32_t prefix_x = 0;
        if (menu->prefix) {
            bm_pango_get_text_extents(cairo, &paint, &result, "%s ", menu->prefix);
            prefix_x += result.x_advance;
        }

        uint32_t scrollbar_w = 0;
        if (scrollbar) {
            bm_pango_get_text_extents(cairo, &paint, &result, "#");
            scrollbar_w = result.x_advance;
            spacing_x += (spacing_x < scrollbar_w ? scrollbar_w : 0);
        }

        const uint32_t page = (menu->index / lines) * lines;
        
        for (uint32_t l = 0, i = page; l < lines && posy < max_height; ++i, ++l) {
            if (!menu->fixed_height && i >= count) {
                continue;
            }


            uint32_t is_fixed_up = (menu->fixed_height && menu->lines_mode == BM_LINES_UP);
            uint32_t required_empty = (count > lines ? 0 : lines - count);
            int32_t last_item_index = (count < lines ? count - 1 + page : lines - 1 + page);
            int32_t display_item_index = (menu->lines_mode == BM_LINES_DOWN ? i : menu->fixed_height ? last_item_index - i + page + required_empty  : last_item_index - i + page);
            display_item_index = (display_item_index < 0 ? 0 : display_item_index);

            bool highlighted = false;
            if ((i < count && !is_fixed_up) || (is_fixed_up && display_item_index <= last_item_index)) {
                highlighted = (items[display_item_index] == bm_menu_get_highlighted_item(menu));
            }

            if (highlighted) {
                if (menu->event_feedback) {
                    bm_cairo_color_from_menu_color(menu, BM_COLOR_FEEDBACK_FG, &paint.fg);
                    bm_cairo_color_from_menu_color(menu, BM_COLOR_FEEDBACK_BG, &paint.bg);
                } else {
                    bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_FG, &paint.fg);
                    bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_BG, &paint.bg);
                }
            } else if (i < count && bm_menu_item_is_selected(menu, items[i])) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_BG, &paint.bg);
            } else if (i < count && i % 2 == 1) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ALTERNATE_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ALTERNATE_BG, &paint.bg);
            } else {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            }
            
            char *line_str = "";
            if ((i < count && !is_fixed_up) || (is_fixed_up && display_item_index <= last_item_index)) {
                line_str = bm_cairo_entry_message(items[display_item_index]->text, highlighted, menu->event_feedback, i,  count);
            }

            if (menu->prefix && highlighted) {
                paint.pos = (struct pos){ spacing_x + border_size, posy + vpadding + border_size };
                paint.box = (struct box){ 4, 0, vpadding, -vpadding, width - paint.pos.x, height };
                bm_cairo_draw_line(cairo, &paint, &result, "%s %s", menu->prefix, line_str);
            } else {
                paint.pos = (struct pos){ spacing_x + border_size, posy+vpadding + border_size };
                paint.box = (struct box){ 4 + prefix_x, 0, vpadding, -vpadding, width - paint.pos.x, height };
                bm_cairo_draw_line(cairo, &paint, &result, "%s", line_str);
            }

            posy += (spacing_y ? spacing_y : result.height);
            out_result->height = posy;
            out_result->displayed++;
            page_length += 1;
        }

        if (spacing_x) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            const uint32_t sheight = out_result->height - titleh;
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, scrollbar_w + border_size, titleh + border_size, spacing_x - scrollbar_w, sheight);
            cairo_fill(cairo->cr);
        }

        if (scrollbar && count > 0) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_SCROLLBAR_BG, &paint.bg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_SCROLLBAR_FG, &paint.fg);

            const uint32_t sheight = out_result->height - titleh;
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, border_size, titleh + border_size, scrollbar_w, sheight);
            cairo_fill(cairo->cr);

            const float percent = fmin(((float)page / (count - lines)), 1.0f);
            const float fraction = fmin((float)lines / count, 1.0f);
            const uint32_t size = fmax(sheight * fraction, 2.0f);
            const uint32_t posy = percent * (sheight - size);
            cairo_set_source_rgba(cairo->cr, paint.fg.r, paint.fg.b, paint.fg.g, paint.fg.a);
            cairo_rectangle(cairo->cr, border_size, titleh + posy + border_size, scrollbar_w, size);
            cairo_fill(cairo->cr);
        }
    } else {
        /* single-line mode */
        bm_pango_get_text_extents(cairo, &paint, &result, "lorem ipsum lorem ipsum lorem ipsum lorem");
        uint32_t cl = fmin(title_x + result.x_advance, width / 4);

        if (count > 0) {
            paint.pos = (struct pos){ cl, vpadding + border_size };
            paint.box = (struct box){ 1, 2, vpadding, -vpadding, 0, height };
            bm_cairo_draw_line(cairo, &paint, &result, (count > 0 && (menu->wrap || menu->index > 0) ? "<" : " "));
            cl += result.x_advance + 1;
        }

        for (uint32_t i = menu->index; i < count && cl < (width/cairo->scale); ++i) { 
            bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));

            if (highlighted) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_BG, &paint.bg);
            } else if (bm_menu_item_is_selected(menu, items[i])) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_BG, &paint.bg);
            } else if (i % 2 == 1) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ALTERNATE_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ALTERNATE_BG, &paint.bg);
            } else {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            }

            uint32_t hpadding = (menu->hpadding == 0 ? 2 : menu->hpadding);
            paint.pos = (struct pos){ cl + (hpadding/2), vpadding + border_size };
            paint.box = (struct box){ hpadding/2, 1.5 * hpadding, vpadding, -vpadding, 0, height };
            bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            cl += result.x_advance + (0.5 * hpadding);
            out_result->displayed += (cl < width);
            out_result->height = fmax(out_result->height, result.height);
        }
        bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
        if (menu->wrap || menu->index + 1 < count) {
            bm_pango_get_text_extents(cairo, &paint, &result, ">");
            paint.pos = (struct pos){ width/cairo->scale - result.x_advance - 2, vpadding + border_size };
            paint.box = (struct box){ 1, 2, vpadding, -vpadding, 0, height };
            bm_cairo_draw_line(cairo, &paint, &result, ">");
        }
        
    }

    if (menu->lines_mode == BM_LINES_UP) {
        if (menu->title) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_FG, &paint.fg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_BG, &paint.bg);
        
            paint.pos = (struct pos){ border_size + 4, posy + vpadding + border_size };
            paint.box = (struct box){ 4, 16, vpadding, -vpadding, 0, height };
            bm_cairo_draw_line(cairo, &paint, &result, "%s", menu->title);
        }

        bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_CURSOR_FG, &paint.cursor_fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_CURSOR_BG, &paint.cursor_bg);
        paint.draw_cursor = true;
        paint.cursor = menu->cursor;
        paint.cursor_height = menu->cursor_height;
        paint.cursor_width = menu->cursor_width;
        result.x_advance = (menu->title ? result.x_advance : 0);
        paint.pos = (struct pos){ (menu->title ? 2 : 0) + result.x_advance + border_size, posy + border_size };
        paint.box = (struct box){ (menu->title ? 2 : 4), 0, vpadding, -vpadding, width - paint.pos.x, height };

        const char *filter_text = (menu->filter ? menu->filter : "");
        if (menu->password == BM_PASSWORD_HIDE) {
            bm_cairo_draw_line_str(cairo, &paint, &result, "");
        } else if (menu->password == BM_PASSWORD_INDICATOR) {
            char asterisk_print[1024] = "";
        
            for (int i = 0; i < (int)(strlen(filter_text)); ++i) asterisk_print[i] = '*';
        
            bm_cairo_draw_line(cairo, &paint, &result, "%s", asterisk_print);
        } else if (menu->password == BM_PASSWORD_NONE) {
            bm_cairo_draw_line(cairo, &paint, &result, "%s", filter_text);
        }

        paint.draw_cursor = false;

        posy += (spacing_y ? spacing_y : result.height);
        out_result->height = posy;
        out_result->displayed++;
    }

    if (menu->counter) {
        char counter[128];
        snprintf(counter, sizeof(counter), "[%u/%u]", filtered_item_count, total_item_count);
        bm_pango_get_text_extents(cairo, &paint, &result, "%s", counter);
        
        bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
        paint.pos = (struct pos){ width/cairo->scale - result.x_advance - 10, vpadding + border_size };
        paint.box = (struct box){ 1, 2, vpadding, -vpadding, 0, height };
        bm_cairo_draw_line(cairo, &paint, &result, "%s", counter);
    }

    // Draw borders
    bm_cairo_color_from_menu_color(menu, BM_COLOR_BORDER, &paint.fg);
    cairo_set_source_rgba(cairo->cr, paint.fg.r, paint.fg.b, paint.fg.g, paint.fg.a);
    if (!border_radius) {
        cairo_rectangle(cairo->cr, 0, 0, (width + border_size)/cairo->scale, total_height);
    } else {
        bm_cairo_rounded_path(cairo->cr, 1, 1, ((width + border_size)/cairo->scale)-2, total_height-2, border_radius);
    }
    cairo_set_line_width(cairo->cr, 2 * menu->border_size);
    
    cairo_stroke(cairo->cr);

    out_result->height += 2 * border_size;
    out_result->height *= cairo->scale;
    cairo_reset_clip(cairo->cr);
}

#endif /* _BM_CAIRO_H */

/* vim: set ts=8 sw=4 tw=0 :*/
