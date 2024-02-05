#include "internal.h"
#include "x11.h"

#include <stdlib.h>
#include <cairo-xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xutil.h>

static void
destroy_buffer(struct buffer *buffer)
{
    bm_cairo_destroy(&buffer->cairo);
    memset(buffer, 0, sizeof(struct buffer));
}

static bool
create_buffer(struct window *window, struct buffer *buffer, int32_t width, int32_t height)
{
    cairo_surface_t *surf;
    if (!(surf = cairo_xlib_surface_create(window->display, window->drawable, window->visual, width, height)))
        goto fail;

    cairo_xlib_surface_set_size(surf, width, height);

    const char *scale = getenv("BEMENU_SCALE");
    if (scale) {
        buffer->cairo.scale = fmax(strtof(scale, NULL), 1.0f);
    } else {
        buffer->cairo.scale = 1;
    }

    if (!bm_cairo_create_for_surface(&buffer->cairo, surf)) {
        cairo_surface_destroy(surf);
        goto fail;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->created = true;
    return true;

fail:
    destroy_buffer(buffer);
    return false;
}

static struct buffer*
next_buffer(struct window *window)
{
    assert(window);

    struct buffer *buffer = &window->buffer;

    if (!buffer)
        return NULL;

    if (window->width != buffer->width || window->height != buffer->height)
        destroy_buffer(buffer);

    if (!buffer->created && !create_buffer(window, buffer, window->width, window->height))
        return NULL;

    return buffer;
}

static uint32_t
get_window_width(struct window *window)
{
    uint32_t width = window->width * ((window->width_factor != 0) ? window->width_factor : 1);

    if(width > window->width - 2 * window->hmargin_size)
        width = window->width - 2 * window->hmargin_size;

    if(width < WINDOW_MIN_WIDTH || 2 * window->hmargin_size > window->width)
        width = WINDOW_MIN_WIDTH;

    return width;
}

void
bm_x11_window_render(struct window *window, struct bm_menu *menu)
{
    assert(window && menu);
    uint32_t oldw = window->width, oldh = window->height;

    struct buffer *buffer;
    for (int32_t tries = 0; tries < 2; ++tries) {
        if (!(buffer = next_buffer(window))) {
            fprintf(stderr, "could not get next buffer");
            exit(EXIT_FAILURE);
        }

        if (!window->notify.render)
            break;

        cairo_push_group(buffer->cairo.cr);
        struct cairo_paint_result result;
        window->notify.render(&buffer->cairo, buffer->width, window->max_height, menu, &result);
        window->displayed = result.displayed;
        cairo_pop_group_to_source(buffer->cairo.cr);

        if (window->height == result.height)
            break;

        window->height = result.height;
        destroy_buffer(buffer);
    }

    if (oldw != window->width || oldh != window->height) {
        uint32_t win_y = 0;

        if(window->align == BM_ALIGN_CENTER) {
                win_y = (window->max_height - window->height) / 2;
        } else if(window->align == BM_ALIGN_BOTTOM) {
                win_y = window->max_height - window->height;
        }

        XMoveResizeWindow(window->display, window->drawable, window->x, win_y + window->y_offset, window->width, window->height);
    }

    if (buffer->created) {
        cairo_save(buffer->cairo.cr);
        cairo_set_operator(buffer->cairo.cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(buffer->cairo.cr);
        cairo_surface_flush(buffer->cairo.surface);
        cairo_restore(buffer->cairo.cr);
    }
}

void
bm_x11_window_key_press(struct window *window, XKeyEvent *ev)
{
    KeySym keysym = NoSymbol;
    XmbLookupString(window->xic, ev, NULL, 0, &keysym, NULL);
    window->mods = 0;
    if (ev->state & ControlMask) window->mods |= MOD_CTRL;
    if (ev->state & ShiftMask) window->mods |= MOD_SHIFT;
    if (ev->state & Mod1Mask) window->mods |= MOD_ALT;
    window->keysym = keysym;
}

void
bm_x11_window_destroy(struct window *window)
{
    assert(window);
    destroy_buffer(&window->buffer);

    if (window->display && window->drawable)
        XDestroyWindow(window->display, window->drawable);
}

void
bm_x11_window_set_monitor(struct window *window, int32_t monitor)
{
    if (window->monitor == monitor)
        return;

    Window root = DefaultRootWindow(window->display);

    {
        /* xinerama logic straight from dmenu */
#define INTERSECT(x,y,w,h,r)  (fmax(0, fmin((x)+(w),(r).x_org+(r).width) - fmax((x),(r).x_org)) * fmax(0, fmin((y)+(h),(r).y_org+(r).height) - fmax((y),(r).y_org)))

        int32_t n;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(window->display, &n))) {
            int32_t x, y, a, j, di, i = 0, area = 0;
            uint32_t du;
            Window w, pw, dw, *dws;
            XWindowAttributes wa;

            XGetInputFocus(window->display, &w, &di);
            if (monitor >= 0 && monitor < n)
                i = monitor;
            else if (w != root && w != PointerRoot && w != None) {
                /* find top-level window containing current input focus */
                do {
                    if (XQueryTree(window->display, (pw = w), &dw, &w, &dws, &du) && dws)
                        XFree(dws);
                } while(w != root && w != pw);

                /* find xinerama screen with which the window intersects most */
                if (XGetWindowAttributes(window->display, pw, &wa)) {
                    for (j = 0; j < n; j++)
                        if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
                            area = a;
                            i = j;
                        }
                }
            }

            /* no focused window is on screen, so use pointer location instead */
            if (monitor < 0 && !area && XQueryPointer(window->display, root, &dw, &dw, &x, &y, &di, &di, &du)) {
                for (i = 0; i < n; i++) {
                    if (INTERSECT(x, y, 1, 1, info[i]) > 0)
                        break;
                }
            }

            window->x = info[i].x_org;
            window->y = info[i].y_org;
            if(window->align == BM_ALIGN_CENTER) {
                window->y += (info[i].height - window->height) / 2;
            } else if(window->align == BM_ALIGN_BOTTOM) {
                window->y += info[i].height - window->height;
            }

            window->width = info[i].width;
            window->max_height = info[i].height;
            XFree(info);
        } else {
            window->max_height = DisplayHeight(window->display, window->screen);
            window->x = 0;
            if(window->align == BM_ALIGN_CENTER) {
                window->y = (window->max_height - window->height) / 2;
            } else if(window->align == BM_ALIGN_BOTTOM) {
                window->y = window->max_height - window->height;
            } else {
                window->y = 0;
            }
            window->width = DisplayWidth(window->display, window->screen);
        }

        window->orig_width = window->width;
        window->orig_x = window->x;
        window->width = get_window_width(window);
        window->x += (window->orig_width - window->width) / 2;

#undef INTERSECT
    }

    window->monitor = monitor;
    XMoveResizeWindow(window->display, window->drawable, window->x, window->y + window->y_offset, window->width, window->height);
    XFlush(window->display);
}

void
bm_x11_window_set_align(struct window *window, enum bm_align align)
{
    if(window->align == align)
        return;

    window->align = align;
    bm_x11_window_set_monitor(window, window->monitor);
}

void
bm_x11_window_set_y_offset(struct window *window, int32_t y_offset)
{
    if(window->y_offset == y_offset)
        return;

    window->y_offset = y_offset;
}

void
bm_x11_window_set_width(struct window *window, uint32_t margin, float factor)
{
    if(window->hmargin_size == margin && window->width_factor == factor)
        return;

    window->hmargin_size = margin;
    window->width_factor = factor;
    window->width = window->orig_width;
    window->x = window->orig_x;
    window->width = get_window_width(window);
    window->x += (window->orig_width - window->width) / 2;
    bm_x11_window_set_monitor(window, window->monitor);
}

bool
bm_x11_window_create(struct window *window, Display *display)
{
    assert(window);

    window->display = display;
    window->screen = DefaultScreen(display);
    window->width = window->height = 1;
    window->monitor = -1;
    window->visual = DefaultVisual(display, window->screen);

    XSetWindowAttributes wa = {
        .override_redirect = True,
        .event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask | ButtonPressMask
    };

    XVisualInfo vinfo;
    int depth = DefaultDepth(display, window->screen);
    unsigned long valuemask = CWOverrideRedirect | CWEventMask | CWBackPixel;

    if (XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo)) {
        depth = vinfo.depth;
        window->visual = vinfo.visual;
        wa.background_pixmap = None;
        wa.border_pixel = 0;
        wa.colormap = XCreateColormap(display, DefaultRootWindow(display), window->visual, AllocNone);
        valuemask = CWOverrideRedirect | CWEventMask | CWBackPixmap | CWColormap | CWBorderPixel;
    }

    window->drawable = XCreateWindow(display, DefaultRootWindow(display), 0, 0, window->width, window->height, 0, depth, CopyFromParent, window->visual, valuemask, &wa);
    XSelectInput(display, window->drawable, ButtonPressMask | KeyPressMask);
    XMapRaised(display, window->drawable);
    window->xim = XOpenIM(display, NULL, NULL, NULL);
    window->xic = XCreateIC(window->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, window->drawable, XNFocusWindow, window->drawable, NULL);
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
