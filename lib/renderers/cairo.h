#ifndef _BM_CAIRO_H_
#define _BM_CAIRO_H_

#include "internal.h"
#include <string.h>
#include <assert.h>
#include <math.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

struct cairo {
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoContext *pango;
};

struct cairo_color {
    float r, g, b, a;
};

struct cairo_paint {
    struct cairo_color fg;
    struct cairo_color bg;
    const char *font;

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
};

struct cairo_paint_result {
    uint32_t displayed;
    uint32_t height;
};

static size_t blen = 0;
static char *buffer = NULL;

__attribute__((unused)) static bool
bm_cairo_create_for_surface(struct cairo *cairo, cairo_surface_t *surface)
{
    assert(cairo && surface);
    if (!(cairo->cr = cairo_create(surface)))
        goto fail;

    if (!(cairo->pango = pango_cairo_create_context(cairo->cr)))
        goto fail;

    cairo->surface = surface;
    return true;

fail:
    if (cairo->cr)
      cairo_destroy(cairo->cr);
    return false;
}

__attribute__((unused)) static void
bm_cairo_destroy(struct cairo *cairo)
{
    if (cairo->cr)
        cairo_destroy(cairo->cr);
    if (cairo->surface)
        cairo_surface_destroy(cairo->surface);
}

__attribute__((unused)) static PangoLayout*
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

__attribute__((unused)) BM_LOG_ATTR(4, 5) static bool
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
    pango_layout_get_pixel_extents(layout, &rect, NULL);
    g_object_unref(layout);

    result->x_advance = rect.x + rect.width;
    result->height = rect.height;
    return true;
}

__attribute__((unused)) BM_LOG_ATTR(4, 5) static bool
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

    PangoLayout *layout = bm_pango_get_layout(cairo, paint, buffer);
    pango_cairo_update_layout(cairo->cr, layout);

    int width, height;
    pango_layout_get_pixel_size(layout, &width, &height);
    int base =  pango_layout_get_baseline(layout) / PANGO_SCALE;
    int yoff = height - base;

    cairo_set_source_rgba(cairo->cr, paint->bg.r, paint->bg.b, paint->bg.g, paint->bg.a);
    cairo_rectangle(cairo->cr,
            paint->pos.x - paint->box.lx, paint->pos.y - paint->box.ty,
            (paint->box.w > 0 ? paint->box.w : width) + paint->box.rx + paint->box.lx,
            (paint->box.h > 0 ? paint->box.h : height) + paint->box.by + paint->box.ty);
    cairo_fill(cairo->cr);

    cairo_set_source_rgba(cairo->cr, paint->fg.r, paint->fg.b, paint->fg.g, paint->fg.a);
    cairo_move_to(cairo->cr, paint->box.lx + paint->pos.x, paint->pos.y - yoff + paint->box.ty);
    pango_cairo_show_layout(cairo->cr, layout);

    g_object_unref(layout);

    result->x_advance = width + paint->box.rx;
    result->height = height + paint->box.by;
    return true;
}

__attribute__((unused)) static void
bm_cairo_color_from_menu_color(const struct bm_menu *menu, enum bm_color color, struct cairo_color *c)
{
    assert(menu);
    c->r = (float)menu->colors[color].r / 255.0f;
    c->g = (float)menu->colors[color].g / 255.0f;
    c->b = (float)menu->colors[color].b / 255.0f;
    c->a = 1.0f;
}

__attribute__((unused)) static void
bm_cairo_paint(struct cairo *cairo, uint32_t width, uint32_t height, uint32_t max_height, const struct bm_menu *menu, struct cairo_paint_result *out_result)
{
    assert(cairo && menu && out_result);

    memset(out_result, 0, sizeof(struct cairo_paint_result));
    out_result->displayed = 1;

    struct cairo_color c;
    bm_cairo_color_from_menu_color(menu, BM_COLOR_BG, &c);
    cairo_set_source_rgb(cairo->cr, c.r, c.g, c.b);
    cairo_rectangle(cairo->cr, 0, 0, width, height);
    cairo_fill(cairo->cr);

    struct cairo_paint paint;
    memset(&paint, 0, sizeof(paint));
    paint.font = menu->font.name;

    struct cairo_result result;
    memset(&result, 0, sizeof(result));

    uint32_t title_x = 0;
    if (menu->title) {
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_BG, &paint.bg);
        paint.pos = (struct pos){ result.x_advance, 2 };
        paint.box = (struct box){ 4, 8, 2, 2, 0, 0 };
        bm_cairo_draw_line(cairo, &paint, &result, "%s", menu->title);
        title_x = result.x_advance;
    }

    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
    paint.pos = (struct pos){ (menu->title ? 2 : 0) + result.x_advance, 2 };
    paint.box = (struct box){ (menu->title ? 2 : 4), 0, 2, 2, width - paint.pos.x, 0 };
    bm_cairo_draw_line(cairo, &paint, &result, "%s", (menu->filter ? menu->filter : ""));

    uint32_t count;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    uint32_t lines = (menu->lines > 0 ? menu->lines : 1);
    uint32_t titleh = result.height;
    out_result->height = titleh;

    if (lines > 1) {
        /* vertical mode */

        uint32_t spacing_x = (menu->scrollbar ? 4 : 0);
        uint32_t spacing_y = 0; // 0 == variable width spacing
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

        uint32_t posy = titleh;
        for (uint32_t l = 0, i = (menu->index / lines) * lines; l < lines && i < count && posy < max_height; ++i, ++l) {
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
                paint.pos = (struct pos){ 0, 2 + posy };
                paint.box = (struct box){ 4 + spacing_x, 0, 2, 2, width - paint.pos.x, 0 };
                bm_cairo_draw_line(cairo, &paint, &result, "%s %s", menu->prefix, (items[i]->text ? items[i]->text : ""));
            } else {
                paint.pos = (struct pos){ 0, 2 + posy };
                paint.box = (struct box){ 4 + spacing_x + prefix_x, 0, 2, 2, width - paint.pos.x, 0 };
                bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            }

            posy += (spacing_y ? spacing_y : result.height);
            out_result->height = posy + 2;
            out_result->displayed++;
        }

        if (menu->scrollbar && count > 0) {
            uint32_t sheight = out_result->height - titleh;
            bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_FG, &paint.bg);
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, 0, titleh, 2, sheight);
            cairo_fill(cairo->cr);

            uint32_t size = sheight / lines;
            uint32_t percent = (menu->index / (float)(count - 1)) * (sheight - size);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_BG, &paint.bg);
            cairo_set_source_rgba(cairo->cr, paint.bg.r, paint.bg.b, paint.bg.g, paint.bg.a);
            cairo_rectangle(cairo->cr, 0, titleh + percent, 2, size);
            cairo_fill(cairo->cr);
        }
    } else {
        /* single-line mode */
        bm_pango_get_text_extents(cairo, &paint, &result, "lorem ipsum lorem ipsum lorem ipsum lorem");
        uint32_t cl = fmin(title_x + result.x_advance, width / 4);

        if (menu->wrap || menu->index > 0) {
            paint.pos = (struct pos){ cl, 2 };
            paint.box = (struct box){ 1, 2, 2, 2, 0, 0 };
            bm_cairo_draw_line(cairo, &paint, &result, "<");
            cl += result.x_advance + 1;
        }

        for (uint32_t i = menu->index; i < count && cl < width; ++i) {
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

            paint.pos = (struct pos){ cl, 2 };
            paint.box = (struct box){ 2, 4, 2, 2, 0, 0 };
            bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            cl += result.x_advance + 2;
            out_result->displayed += (cl < width);
            out_result->height = fmax(out_result->height, result.height);
        }

        if (menu->wrap || menu->index + 1 < count) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
            bm_pango_get_text_extents(cairo, &paint, &result, ">");
            paint.pos = (struct pos){ width - result.x_advance - 2, 2 };
            paint.box = (struct box){ 1, 2, 2, 2, 0, 0 };
            bm_cairo_draw_line(cairo, &paint, &result, ">");
        }
    }
}

#endif /* _BM_CAIRO_H */

/* vim: set ts=8 sw=4 tw=0 :*/
