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
    int scale;
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

        uint32_t cursor_height = fmin(paint->cursor_height == 0 ? line_height : paint->cursor_height, line_height);
        cairo_set_source_rgba(cairo->cr, paint->fg.r, paint->fg.b, paint->fg.g, paint->fg.a);
        cairo_rectangle(cairo->cr,
                paint->pos.x + paint->box.lx + rect.x / PANGO_SCALE, paint->pos.y - paint->box.ty + ((line_height - cursor_height) / 2),
                rect.width / PANGO_SCALE, cursor_height);
        cairo_fill(cairo->cr);

        cairo_rectangle(cairo->cr,
                paint->pos.x + paint->box.lx + rect.x / PANGO_SCALE, paint->pos.y - paint->box.ty,
                rect.width / PANGO_SCALE, line_height);
        cairo_clip(cairo->cr);

        cairo_set_source_rgba(cairo->cr, paint->bg.r, paint->bg.b, paint->bg.g, paint->bg.a);
        cairo_move_to(cairo->cr, paint->box.lx + paint->pos.x, paint->pos.y - base + paint->baseline);
        pango_cairo_show_layout(cairo->cr, layout);
        cairo_reset_clip(cairo->cr);
    }

    g_object_unref(layout);

    result->x_advance = width + paint->box.rx;
    result->height = line_height;

    cairo_identity_matrix(cairo->cr);
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

static inline void
bm_cairo_paint(struct cairo *cairo, uint32_t width, uint32_t max_height, const struct bm_menu *menu, struct cairo_paint_result *out_result)
{
    assert(cairo && menu && out_result);

    max_height /= cairo->scale;
    uint32_t height = fmin(menu->line_height, max_height);

    memset(out_result, 0, sizeof(struct cairo_paint_result));
    out_result->displayed = 1;

    cairo_set_source_rgba(cairo->cr, 0, 0, 0, 0);
    cairo_rectangle(cairo->cr, 0, 0, width, height);

    cairo_save(cairo->cr);
    cairo_set_operator(cairo->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cairo->cr);
    cairo_restore(cairo->cr);

    struct cairo_paint paint = {0};
    paint.font = menu->font.name;

    struct cairo_result result = {0};
    int ascii_height;
    bm_pango_get_text_extents(cairo, &paint, &result, "!\"#$%%&'()*+,-./0123456789:;<=>?@ABCD"
                              "EFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
    ascii_height = result.height;
    paint.baseline = result.baseline;

    int32_t vpadding_t = height == 0 ? 2 : (height - ascii_height) / 2;
	int32_t vpadding_b = vpadding_t + (height - ascii_height) % 2;

    memset(&result, 0, sizeof(result));
    uint32_t title_x = 0;
    if (menu->title) {
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_BG, &paint.bg);
        paint.pos = (struct pos){ result.x_advance, vpadding_t };
        paint.box = (struct box){ 4, 8, vpadding_t, vpadding_b, 0, ascii_height };
        bm_cairo_draw_line(cairo, &paint, &result, "%s", menu->title);
        title_x = result.x_advance;
    }

    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
    paint.draw_cursor = true;
    paint.cursor = menu->cursor;
    paint.cursor_height = menu->cursor_height;
    paint.pos = (struct pos){ (menu->title ? 2 : 0) + result.x_advance, vpadding_t };
    paint.box = (struct box){ (menu->title ? 2 : 4), 0, vpadding_t, vpadding_b, width - paint.pos.x, ascii_height };

    const char *filter_text = (menu->filter ? menu->filter : "");
    if (menu->password) {
        bm_cairo_draw_line_str(cairo, &paint, &result, "");
    } else {
        bm_cairo_draw_line(cairo, &paint, &result, "%s", filter_text);
    }

    paint.draw_cursor = false;
    const uint32_t titleh = result.height;
    out_result->height = titleh;

    uint32_t count;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    uint32_t lines = (menu->lines > 0 ? menu->lines : 1);

    if (menu->lines > 0) {
        /* vertical mode */

        const bool scrollbar = (menu->scrollbar > BM_SCROLLBAR_NONE && (menu->scrollbar != BM_SCROLLBAR_AUTOHIDE || count > lines) ? true : false);
        uint32_t spacing_x = menu->spacing ? title_x : 0, spacing_y = 0; // 0 == variable width spacing
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

        uint32_t posy = titleh;
        const uint32_t page = (menu->index / lines) * lines;
        for (uint32_t l = 0, i = page; l < lines && i < count && posy < max_height; ++i, ++l) {
            bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));

            if (highlighted) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_HIGHLIGHTED_BG, &paint.bg);
            } else if (bm_menu_item_is_selected(menu, items[i])) {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_SELECTED_BG, &paint.bg);
            } else {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            }

            if (menu->prefix && highlighted) {
                paint.pos = (struct pos){ spacing_x, vpadding_t + posy };
                paint.box = (struct box){ 4, 0, vpadding_t, vpadding_b, width - paint.pos.x, ascii_height };
                bm_cairo_draw_line(cairo, &paint, &result, "%s %s", menu->prefix, (items[i]->text ? items[i]->text : ""));
            } else {
                paint.pos = (struct pos){ spacing_x, vpadding_t + posy };
                paint.box = (struct box){ 4 + prefix_x, 0, vpadding_t, vpadding_b, width - paint.pos.x, ascii_height };
                bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            }

            posy += (spacing_y ? spacing_y : result.height);
            out_result->height = posy;
            out_result->displayed++;
        }

        if (spacing_x) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            const uint32_t sheight = out_result->height - titleh;
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, scrollbar_w, titleh, spacing_x - scrollbar_w, sheight);
            cairo_fill(cairo->cr);
        }

        if (scrollbar && count > 0) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_SCROLLBAR_BG, &paint.bg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_SCROLLBAR_FG, &paint.fg);

            const uint32_t sheight = out_result->height - titleh;
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, 0, titleh, scrollbar_w, sheight);
            cairo_fill(cairo->cr);

            const float percent = fmin(((float)page / (count - lines)), 1.0f);
            const uint32_t size = fmax(sheight * ((float)lines / count), 2.0f);
            const uint32_t posy = percent * (sheight - size);
            cairo_set_source_rgba(cairo->cr, paint.fg.r, paint.fg.b, paint.fg.g, paint.fg.a);
            cairo_rectangle(cairo->cr, 0, titleh + posy, scrollbar_w, size);
            cairo_fill(cairo->cr);
        }
    } else {
        /* single-line mode */
        bm_pango_get_text_extents(cairo, &paint, &result, "lorem ipsum lorem ipsum lorem ipsum lorem");
        uint32_t cl = fmin(title_x + result.x_advance, width / 4);

        if (count > 0) {
            paint.pos = (struct pos){ cl, vpadding_t };
            paint.box = (struct box){ 1, 2, vpadding_t, vpadding_b, 0, ascii_height };
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
            } else {
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_FG, &paint.fg);
                bm_cairo_color_from_menu_color(menu, BM_COLOR_ITEM_BG, &paint.bg);
            }

            paint.pos = (struct pos){ cl, vpadding_t };
            paint.box = (struct box){ 2, 4, vpadding_t, vpadding_b, 0, ascii_height };
            bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            cl += result.x_advance + 2;
            out_result->displayed += (cl < width);
            out_result->height = fmax(out_result->height, result.height);
        }

        if (menu->wrap || menu->index + 1 < count) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
            bm_pango_get_text_extents(cairo, &paint, &result, ">");
            paint.pos = (struct pos){ width/cairo->scale - result.x_advance - 2, vpadding_t };
            paint.box = (struct box){ 1, 2, vpadding_t, vpadding_b, 0, ascii_height };
            bm_cairo_draw_line(cairo, &paint, &result, ">");
        }
    }

    out_result->height *= cairo->scale;
}

#endif /* _BM_CAIRO_H */

/* vim: set ts=8 sw=4 tw=0 :*/
