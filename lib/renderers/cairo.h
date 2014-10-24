#ifndef _BM_CAIRO_H_
#define _BM_CAIRO_H_

#include "internal.h"
#include <string.h>
#include <assert.h>
#include <cairo/cairo.h>

struct cairo {
    cairo_t *cr;
    cairo_surface_t *surface;
};

struct cairo_color {
    float r, g, b, a;
};

struct cairo_font {
    const char *name;
    uint32_t size;
};

struct cairo_paint {
    struct cairo_color color;
    struct cairo_font font;
};

struct cairo_result {
    cairo_text_extents_t te;
};

__attribute__((unused)) BM_LOG_ATTR(6, 7) static bool
bm_cairo_draw_line(struct cairo *cairo, struct cairo_paint *paint, struct cairo_result *result, int32_t x, int32_t y, const char *fmt, ...)
{
    static size_t blen = 0;
    static char *buffer = NULL;
    assert(cairo && paint && result && fmt);
    memset(result, 0, sizeof(struct cairo_result));

    va_list args;
    va_start(args, fmt);
    bool ret = bm_vrprintf(&buffer, &blen, fmt, args);
    va_end(args);

    if (!ret)
        return false;

    cairo_set_source_rgba(cairo->cr, paint->color.r, paint->color.b, paint->color.g, paint->color.a);

    cairo_text_extents_t te;
    cairo_select_font_face(cairo->cr, paint->font.name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo->cr, paint->font.size);
    cairo_text_extents(cairo->cr, buffer, &te);
    cairo_move_to(cairo->cr, x, y * te.height + te.height);
    cairo_show_text(cairo->cr, buffer);

    memcpy(&result->te, &te, sizeof(te));
    return true;
}

__attribute__((unused)) static void
bm_cairo_paint(struct cairo *cairo, uint32_t width, uint32_t height, const struct bm_menu *menu)
{
    cairo_set_source_rgb(cairo->cr, 18.0f / 255.0f, 18 / 255.0f, 18.0f / 255.0f);
    cairo_rectangle(cairo->cr, 0, 0, width, height);
    cairo_fill(cairo->cr);

    struct cairo_result result;
    memset(&result, 0, sizeof(result));

    struct cairo_paint paint;
    memset(&paint, 0, sizeof(paint));
    paint.font.name = "Terminus";
    paint.font.size = 12;
    paint.color = (struct cairo_color){ 1.0, 0.0, 0.0, 1.0 };

    if (menu->title)
        bm_cairo_draw_line(cairo, &paint, &result, result.te.x_advance, 0, "%s ", menu->title);
    if (menu->filter)
        bm_cairo_draw_line(cairo, &paint, &result, result.te.x_advance, 0, "%s", menu->filter);

    uint32_t count, cl = 1;
    uint32_t lines = height / paint.font.size;
    struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
    for (uint32_t i = (menu->index / (lines - 1)) * (lines - 1); i < count && cl < lines; ++i) {
        bool highlighted = (items[i] == bm_menu_get_highlighted_item(menu));

        if (highlighted)
            paint.color = (struct cairo_color){ 1.0, 0.0, 0.0, 1.0 };
        else if (bm_menu_item_is_selected(menu, items[i]))
            paint.color = (struct cairo_color){ 1.0, 0.0, 0.0, 1.0 };
        else
            paint.color = (struct cairo_color){ 1.0, 1.0, 1.0, 1.0 };

        bm_cairo_draw_line(cairo, &paint, &result, 0, cl++, "%s%s", (highlighted ? ">> " : "   "), (items[i]->text ? items[i]->text : ""));
    }
}

#endif /* _BM_CAIRO_H */

/* vim: set ts=8 sw=4 tw=0 :*/
