#ifndef _BM_CAIRO_H_
#define _BM_CAIRO_H_

#include "internal.h"
#include <string.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#ifndef MAX
#  define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef MIN
#  define MIN(a,b) (((a)<(b))?(a):(b))
#endif

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
    cairo_font_extents_t fe;

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
    cairo_text_extents_t te;
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

__attribute__((unused)) static void
bm_cairo_get_font_extents(struct cairo *cairo, const struct bm_font *font, cairo_font_extents_t *fe)
{
    assert(cairo && font && fe);
    cairo_select_font_face(cairo->cr, font->name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo->cr, font->size);
    cairo_font_extents(cairo->cr, fe);
}

__attribute__((unused)) BM_LOG_ATTR(3, 4) static bool
bm_cairo_get_text_extents(struct cairo *cairo, struct cairo_result *result, const char *fmt, ...)
{
    assert(cairo && result && fmt);
    memset(result, 0, sizeof(struct cairo_result));

    va_list args;
    va_start(args, fmt);
    bool ret = bm_vrprintf(&buffer, &blen, fmt, args);
    va_end(args);

    if (!ret)
        return false;

    cairo_text_extents(cairo->cr, buffer, &result->te);
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

    PangoLayout *layout = pango_cairo_create_layout(cairo->cr);
    pango_layout_set_text(layout, buffer, -1);
    PangoFontDescription *desc = pango_font_description_from_string("Terminus 12");
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_cairo_update_layout(cairo->cr, layout);

    int width, height;
    pango_layout_get_size(layout, &width, &height);

    cairo_set_source_rgba(cairo->cr, paint->bg.r, paint->bg.b, paint->bg.g, paint->bg.a);
    cairo_rectangle(cairo->cr,
            paint->pos.x - paint->box.lx, paint->pos.y - paint->box.ty,
            (paint->box.w > 0 ? paint->box.w : width / PANGO_SCALE) + paint->box.rx + paint->box.lx,
            (paint->box.h > 0 ? paint->box.h : paint->fe.height) + paint->box.by + paint->box.ty);
    cairo_fill(cairo->cr);

    cairo_set_source_rgba(cairo->cr, paint->fg.r, paint->fg.b, paint->fg.g, paint->fg.a);
    cairo_move_to(cairo->cr, paint->box.lx + paint->pos.x, paint->pos.y - ((height * 0.25) / PANGO_SCALE) + paint->box.ty);
    pango_cairo_show_layout(cairo->cr, layout);
    g_object_unref(layout);

    cairo_text_extents_t te;
    cairo_text_extents(cairo->cr, buffer, &te);

    te.x_advance = width / PANGO_SCALE + paint->box.rx;
    memcpy(&result->te, &te, sizeof(te));
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

__attribute__((unused)) static uint32_t
bm_cairo_paint(struct cairo *cairo, uint32_t width, uint32_t height, const struct bm_menu *menu)
{
    assert(cairo && menu);

    struct cairo_color c;
    bm_cairo_color_from_menu_color(menu, BM_COLOR_BG, &c);
    cairo_set_source_rgb(cairo->cr, c.r, c.g, c.b);
    cairo_rectangle(cairo->cr, 0, 0, width, height);
    cairo_fill(cairo->cr);

    struct cairo_paint paint;
    memset(&paint, 0, sizeof(paint));
    bm_cairo_get_font_extents(cairo, &menu->font, &paint.fe);

    struct cairo_result result;
    memset(&result, 0, sizeof(result));

    if (menu->title) {
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_FG, &paint.fg);
        bm_cairo_color_from_menu_color(menu, BM_COLOR_TITLE_BG, &paint.bg);
        paint.pos = (struct pos){ result.te.x_advance, 2 };
        paint.box = (struct box){ 4, 8, 2, 2, 0, 0 };
        bm_cairo_draw_line(cairo, &paint, &result, "%s", menu->title);
    }

    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
    bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
    paint.pos = (struct pos){ (menu->title ? 2 : 0) + result.te.x_advance, 2 };
    paint.box = (struct box){ (menu->title ? 2 : 4), 0, 2, 2, width - paint.pos.x, 0 };
    bm_cairo_draw_line(cairo, &paint, &result, "%s", (menu->filter ? menu->filter : ""));

    uint32_t displayed = 1;
    uint32_t lines = MAX(height / (paint.fe.height + 4), 1);
    if (lines > 1) {
        uint32_t start_x = 0;
        if (menu->prefix) {
            bm_cairo_get_text_extents(cairo, &result, "%s ", menu->prefix);
            start_x = result.te.x_advance;
        }

        uint32_t count, cl = 1;
        struct bm_item **items = bm_menu_get_filtered_items(menu, &count);
        for (uint32_t i = (menu->index / (lines - 1)) * (lines - 1); i < count && cl < lines; ++i) {
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
                paint.pos = (struct pos){ 0, 2 + (paint.fe.height + 4) * cl++ };
                paint.box = (struct box){ 4, 0, 2, 2, width - paint.pos.x, 0 };
                bm_cairo_draw_line(cairo, &paint, &result, "%s %s", menu->prefix, (items[i]->text ? items[i]->text : ""));
            } else {
                paint.pos = (struct pos){ 0, 2 + (paint.fe.height + 4) * cl++ };
                paint.box = (struct box){ 4 + start_x, 0, 2, 2, width - paint.pos.x, 0 };
                bm_cairo_draw_line(cairo, &paint, &result, "%s", (items[i]->text ? items[i]->text : ""));
            }

            ++displayed;
        }
    } else {
        uint32_t count;
        struct bm_item **items = bm_menu_get_filtered_items(menu, &count);

        uint32_t cl = width / 4;
        if (menu->wrap || menu->index > 0) {
            paint.pos = (struct pos){ cl, 2 };
            paint.box = (struct box){ 1, 2, 2, 2, 0, 0 };
            bm_cairo_draw_line(cairo, &paint, &result, "<");
            cl += result.te.x_advance + 1;
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
            cl += result.te.x_advance + 2;
            displayed += (cl < width);
        }

        if (menu->wrap || menu->index + 1 < count) {
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_FG, &paint.fg);
            bm_cairo_color_from_menu_color(menu, BM_COLOR_FILTER_BG, &paint.bg);
            bm_cairo_get_text_extents(cairo, &result, ">");
            paint.pos = (struct pos){ width - result.te.x_advance - 2, 2 };
            paint.box = (struct box){ 1, 2, 2, 2, 0, 0 };
            bm_cairo_draw_line(cairo, &paint, &result, ">");
        }
    }

    return displayed;
}

#endif /* _BM_CAIRO_H */

/* vim: set ts=8 sw=4 tw=0 :*/
