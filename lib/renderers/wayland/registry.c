#include "wayland.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <string.h>

const char *BM_XKB_MASK_NAMES[MASK_LAST] = {
    XKB_MOD_NAME_SHIFT,
    XKB_MOD_NAME_CAPS,
    XKB_MOD_NAME_CTRL,
    XKB_MOD_NAME_ALT,
    "Mod2",
    "Mod3",
    XKB_MOD_NAME_LOGO,
    "Mod5",
};

const enum mod_bit BM_XKB_MODS[MASK_LAST] = {
    MOD_SHIFT,
    MOD_CAPS,
    MOD_CTRL,
    MOD_ALT,
    MOD_MOD2,
    MOD_MOD3,
    MOD_LOGO,
    MOD_MOD5
};

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
    (void)wl_shm;
    struct wayland *wayland = data;
    wayland->formats |= (1 << format);
}

struct wl_shm_listener shm_listener = {
    .format = shm_format
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
{
    (void)keyboard;
    struct input *input = data;

    if (!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_str;
    if ((map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
        close(fd);
        return;
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(input->xkb.context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    munmap(map_str, size);
    close(fd);

    if (!keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return;
    }

    struct xkb_state *state;
    if (!(state = xkb_state_new(keymap))) {
        fprintf(stderr, "failed to create XKB state\n");
        xkb_keymap_unref(keymap);
        return;
    }

    xkb_keymap_unref(input->xkb.keymap);
    xkb_state_unref(input->xkb.state);
    input->xkb.keymap = keymap;
    input->xkb.state = state;

    for (uint32_t i = 0; i < MASK_LAST; ++i)
        input->xkb.masks[i] = 1 << xkb_keymap_mod_get_index(input->xkb.keymap, BM_XKB_MASK_NAMES[i]);
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
    (void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface)
{
    (void)keyboard, (void)serial, (void)surface;
    struct input *input = data;
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    timerfd_settime(*input->repeat_fd, 0, &its, NULL);
}

static void
press(struct input *input, xkb_keysym_t sym, uint32_t key, enum wl_keyboard_key_state state)
{
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        input->sym = sym;
        input->code = key + 8;
        input->key_pending = true;
    } else if (!input->key_pending) {
        input->sym = XKB_KEY_NoSymbol;
        input->code = 0;
    }

    if (input->notify.key)
        input->notify.key(state, sym, key);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state_w)
{
    (void)keyboard, (void)serial, (void)time;
    struct input *input = data;
    enum wl_keyboard_key_state state = state_w;

    if (!input->xkb.state)
        return;

    xkb_keysym_t sym = xkb_state_key_get_one_sym(input->xkb.state, key + 8);
    press(input, sym, key, state);

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && xkb_keymap_key_repeats(input->xkb.keymap, input->code)) {
        struct itimerspec its;
        input->repeat_sym = sym;
        input->repeat_key = key;
        its.it_interval.tv_sec = input->repeat_rate_sec;
        its.it_interval.tv_nsec = input->repeat_rate_nsec;
        its.it_value.tv_sec = input->repeat_delay_sec;
        its.it_value.tv_nsec = input->repeat_delay_nsec;
        timerfd_settime(*input->repeat_fd, 0, &its, NULL);
    } else if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == input->repeat_key) {
        struct itimerspec its;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
        timerfd_settime(*input->repeat_fd, 0, &its, NULL);
    }
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    (void)keyboard, (void)serial;
    struct input *input = data;

    if (!input->xkb.keymap)
        return;

    xkb_state_update_mask(input->xkb.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    xkb_mod_mask_t mask = xkb_state_serialize_mods(input->xkb.state, XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);

    input->modifiers = 0;
    for (uint32_t i = 0; i < MASK_LAST; ++i) {
        if (mask & input->xkb.masks[i])
            input->modifiers |= BM_XKB_MODS[i];
    }
}

static void
set_repeat_info(struct input *input, int32_t rate, int32_t delay)
{
    assert(input);

    input->repeat_rate_sec = input->repeat_rate_nsec = 0;
    input->repeat_delay_sec = input->repeat_delay_nsec = 0;

    /* a rate of zero disables any repeating, regardless of the delay's value */
    if (rate == 0)
        return;

    if (rate == 1)
        input->repeat_rate_sec = 1;
    else
        input->repeat_rate_nsec = 1000000000 / rate;

    input->repeat_delay_sec = delay / 1000;
    delay -= (input->repeat_delay_sec * 1000);
    input->repeat_delay_nsec = delay * 1000 * 1000;
}

static void
keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay)
{
    (void)keyboard;
    set_repeat_info(data, rate, delay);
}

static void
pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
               uint32_t serial, struct wl_surface *surface,
               wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    (void)wl_pointer, (void)surface;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_ENTER;
    input->pointer_event.serial = serial;
    input->pointer_event.surface_x = surface_x,
    input->pointer_event.surface_y = surface_y;
}

static void
pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
               uint32_t serial, struct wl_surface *surface)
{
    (void)wl_pointer, (void)surface;
    struct input *input = data;
    input->pointer_event.serial = serial;
    input->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

static void
pointer_handle_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time,
               wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_MOTION;
    input->pointer_event.time = time;
    input->pointer_event.surface_x = surface_x,
    input->pointer_event.surface_y = surface_y;
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
               uint32_t time, uint32_t button, uint32_t state)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
    input->pointer_event.time = time;
    input->pointer_event.serial = serial;
    input->pointer_event.button = button,
    input->pointer_event.state |= state;
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time,
               uint32_t axis, wl_fixed_t value)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_AXIS;
    input->pointer_event.time = time;
    input->pointer_event.axes[axis].valid = true;
    input->pointer_event.axes[axis].value = value;
}

static void
pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer,
               uint32_t axis_source)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
    input->pointer_event.axis_source = axis_source;
}

static void
pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer,
               uint32_t time, uint32_t axis)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.time = time;
    input->pointer_event.event_mask |= POINTER_EVENT_AXIS_STOP;
    input->pointer_event.axes[axis].valid = true;
}

static void
pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer,
               uint32_t axis, int32_t discrete)
{
    (void)wl_pointer;
    struct input *input = data;
    input->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
    input->pointer_event.axes[axis].valid = true;
    input->pointer_event.axes[axis].discrete = discrete;
}


static void
pointer_handle_frame(void *data, struct wl_pointer *wl_pointer)
{
    (void) data, (void) wl_pointer;
}

static struct touch_point *
get_touch_point(struct input *input, int32_t id)
{
    struct touch_event *touch = &input->touch_event;
    int invalid = -1;
    for (size_t i = 0; i < 2; ++i) {
        if (touch->points[i].id == id) {
            invalid = i;
        }
        if (invalid == -1 && !touch->points[i].valid) {
            invalid = i;
        }
    }
    if (invalid == -1) {
        return NULL;
    }
    touch->points[invalid].id = id;
    return &touch->points[invalid];
}

static void
reset_all_start_position(struct input *input)
{
    struct touch_event *event = &input->touch_event;
    for (size_t i = 0; i < 2; ++i) {
        struct touch_point *point = &event->points[i];

        if (!point->valid)
            continue;

        point->surface_start_x = point->surface_x;
        point->surface_start_y = point->surface_y;
    }
}

static void
revalidate_all_released(struct input *input)
{
    struct touch_event *event = &input->touch_event;
    for (size_t i = 0; i < 2; ++i) {
        struct touch_point *point = &event->points[i];

        if (!point->valid && point->event_mask & TOUCH_EVENT_DOWN)
            point->valid = true;
    }
}

static void
touch_handle_down(void *data, struct wl_touch *wl_touch, uint32_t serial,
    uint32_t time, struct wl_surface *surface, int32_t id,
    wl_fixed_t x, wl_fixed_t y)
{
    (void) wl_touch, (void) surface;
    struct input *input = data;
    struct touch_point *point = get_touch_point(input, id);
    if (point == NULL) {
        return;
    }
    point->valid = true;
    point->event_mask = TOUCH_EVENT_DOWN;
    point->surface_x = x,
    point->surface_y = y;
    input->touch_event.time = time;
    input->touch_event.serial = serial;
    input->touch_event.active += 1;

    revalidate_all_released(input);
    reset_all_start_position(input);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch, uint32_t serial,
               uint32_t time, int32_t id)
{
    (void) time, (void) wl_touch, (void) serial;
    struct input *input = data;
    struct touch_point *point = get_touch_point(input, id);
    if (point == NULL) {
        return;
    }
    point->event_mask |= TOUCH_EVENT_UP;
    input->touch_event.active -= 1;

    reset_all_start_position(input);
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch, uint32_t time,
               int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    (void) wl_touch;
    struct input *input = data;
    struct touch_point *point = get_touch_point(input, id);
    if (point == NULL) {
        return;
    }
    point->event_mask |= TOUCH_EVENT_MOTION;
    point->surface_x = x, point->surface_y = y;
    input->touch_event.time = time;
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
    (void) wl_touch, (void) data;
}

static void
touch_handle_shape(void *data, struct wl_touch *wl_touch,
               int32_t id, wl_fixed_t major, wl_fixed_t minor)
{
    (void) wl_touch;
    struct input *input = data;
    struct touch_point *point = get_touch_point(input, id);
    if (point == NULL) {
        return;
    }
    point->event_mask |= TOUCH_EVENT_SHAPE;
    point->major = major, point->minor = minor;
}

static void
touch_handle_orientation(void *data, struct wl_touch *wl_touch,
               int32_t id, wl_fixed_t orientation)
{
    (void) wl_touch;
    struct input *input = data;
    struct touch_point *point = get_touch_point(input, id);
    if (point == NULL) {
        return;
    }
    point->event_mask |= TOUCH_EVENT_ORIENTATION;
    point->orientation = orientation;
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
    (void) data, (void) wl_touch;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
    .frame = pointer_handle_frame,
    .axis_source = pointer_handle_axis_source,
    .axis_stop = pointer_handle_axis_stop,
    .axis_discrete = pointer_handle_axis_discrete,
};

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info
};

static const struct wl_touch_listener wl_touch_listener = {
    .down = touch_handle_down,
    .up = touch_handle_up,
    .motion = touch_handle_motion,
    .frame = touch_handle_frame,
    .cancel = touch_handle_cancel,
    .shape = touch_handle_shape,
    .orientation = touch_handle_orientation,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    struct input *input = data;

    if (!input->seat) {
        input->seat = seat;
    }

    if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
        input->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(input->keyboard, &keyboard_listener, data);
    }

    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        input->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(input->pointer, &pointer_listener, data);
    }

    if (caps & WL_SEAT_CAPABILITY_TOUCH) {
        input->touch = wl_seat_get_touch(seat);
        wl_touch_add_listener(input->touch, &wl_touch_listener, data);
    }

    if (seat == input->seat && !(caps & WL_SEAT_CAPABILITY_KEYBOARD) && !(caps & WL_SEAT_CAPABILITY_POINTER)) {
        wl_keyboard_destroy(input->keyboard);
        input->seat = NULL;
        input->keyboard = NULL;
        input->pointer = NULL;
        input->touch = NULL;
    }
}

static void
seat_handle_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name
};

static void
display_handle_geometry(void *data, struct wl_output *wl_output, int x, int y, int physical_width, int physical_height, int subpixel, const char *make, const char *model, int transform)
{
    (void)data, (void)wl_output, (void)x, (void)y, (void)physical_width, (void)physical_height, (void)subpixel, (void)make, (void)model, (void)transform;
}

static void
display_handle_done(void *data, struct wl_output *wl_output)
{
    (void)data, (void)wl_output;
}

static void
display_handle_scale(void *data, struct wl_output *wl_output, int32_t scale)
{
    (void)wl_output;
    struct output *output = data;

    assert(scale > 0);
    output->scale = scale;
}

static void
display_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags, int width, int height, int refresh)
{
    (void)wl_output, (void)refresh, (void)height, (void)width;
    struct output *output = data;

    if (flags & WL_OUTPUT_MODE_CURRENT) {
        output->height = height;
    }
}

static void
display_handle_name(void *data, struct wl_output *wl_output, const char *name)
{
    (void)wl_output;
    struct output *output = data;

    output->name = bm_strdup(name);
}

static void
display_handle_description(void *data, struct wl_output *wl_output, const char *description)
{
    (void)data, (void)wl_output, (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = display_handle_geometry,
    .mode = display_handle_mode,
    .done = display_handle_done,
    .scale = display_handle_scale,
    .name = display_handle_name,
    .description = display_handle_description,
};

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    (void)version;
    struct wayland *wayland = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        wayland->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        wayland->layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, 3);
    } else if (strcmp(interface, "wl_seat") == 0) {
        wayland->seat = wl_registry_bind(registry, id, &wl_seat_interface, 7);
        wl_seat_add_listener(wayland->seat, &seat_listener, &wayland->input);
    } else if (strcmp(interface, "wl_shm") == 0) {
        wayland->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        wl_shm_add_listener(wayland->shm, &shm_listener, data);
    } else if (strcmp(interface, "wl_output") == 0) {
        struct wl_output *wl_output = wl_registry_bind(registry, id, &wl_output_interface, 4);
        struct output *output = calloc(1, sizeof(struct output));
        output->output = wl_output;
        wl_list_insert(&wayland->outputs, &output->link);
        wl_output_add_listener(wl_output, &output_listener, output);
    } else if (strcmp(interface, "wp_fractional_scale_manager_v1") == 0) {
        wayland->wfs_mgr = wl_registry_bind(registry, id, &wp_fractional_scale_manager_v1_interface, 1);
    } else if (strcmp(interface, "wp_viewporter") == 0) {
        wayland->viewporter = wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
    }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

void
bm_wl_repeat(struct wayland *wayland)
{
    uint64_t exp;
    if (read(wayland->fds.repeat, &exp, sizeof(exp)) != sizeof(exp))
        return;

    if (wayland->input.notify.key)
        wayland->input.notify.key(WL_KEYBOARD_KEY_STATE_PRESSED, wayland->input.repeat_sym, wayland->input.repeat_key + 8);

    press(&wayland->input, wayland->input.repeat_sym, wayland->input.repeat_key, WL_KEYBOARD_KEY_STATE_PRESSED);
}

void
bm_wl_registry_destroy(struct wayland *wayland)
{
    assert(wayland);

    if (wayland->shm)
        wl_shm_destroy(wayland->shm);

    if (wayland->layer_shell)
        zwlr_layer_shell_v1_destroy(wayland->layer_shell);

    if (wayland->compositor)
        wl_compositor_destroy(wayland->compositor);

    if (wayland->registry)
        wl_registry_destroy(wayland->registry);

    xkb_keymap_unref(wayland->input.xkb.keymap);
    xkb_state_unref(wayland->input.xkb.state);
}

bool
bm_wl_registry_register(struct wayland *wayland)
{
    assert(wayland);

    if (!(wayland->registry = wl_display_get_registry(wayland->display)))
        return false;

    wl_registry_add_listener(wayland->registry, &registry_listener, wayland);
    wl_display_roundtrip(wayland->display); // trip 1, registry globals
    if (!wayland->compositor || !wayland->seat || !wayland->shm || !wayland->layer_shell)
        return false;

    wl_display_roundtrip(wayland->display); // trip 2, global listeners
    if (!wayland->input.keyboard || !(wayland->formats & (1 << WL_SHM_FORMAT_ARGB8888)))
        return false;

    if (wayland->wfs_mgr && wayland->viewporter) {
        // This feature does not work with some compositors rn so mut be enabled with env var
        const char *env = getenv("BEMENU_WL_FRACTIONAL_SCALING");
        if (env && (!strcmp(env, "1") || !strcmp(env, "true"))) {
            wayland->fractional_scaling = true;
        }
    }

    set_repeat_info(&wayland->input, 40, 400);
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
