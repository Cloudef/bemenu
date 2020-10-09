#include "wayland.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/timerfd.h>

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
    if ((map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
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

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_handle_keymap,
    .enter = keyboard_handle_enter,
    .leave = keyboard_handle_leave,
    .key = keyboard_handle_key,
    .modifiers = keyboard_handle_modifiers,
    .repeat_info = keyboard_handle_repeat_info
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat, enum wl_seat_capability caps)
{
    struct input *input = data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
        input->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(input->keyboard, &keyboard_listener, data);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
        wl_keyboard_destroy(input->keyboard);
        input->keyboard = NULL;
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

static const struct wl_output_listener output_listener = {
    .geometry = display_handle_geometry,
    .mode = display_handle_mode,
    .done = display_handle_done,
    .scale = display_handle_scale
};

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    (void)version;
    struct wayland *wayland = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        wayland->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 3);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        wayland->layer_shell = wl_registry_bind(registry, id, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
        wayland->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
        wl_seat_add_listener(wayland->seat, &seat_listener, &wayland->input);
    } else if (strcmp(interface, "wl_shm") == 0) {
        wayland->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
        wl_shm_add_listener(wayland->shm, &shm_listener, data);
    } else if (strcmp(interface, "wl_output") == 0) {
        struct wl_output *wl_output = wl_registry_bind(registry, id, &wl_output_interface, 2);
        struct output *output = calloc(1, sizeof(struct output));
        output->output = wl_output;
        output->scale = 1;
        wl_list_insert(&wayland->outputs, &output->link);
        wl_output_add_listener(wl_output, &output_listener, output);
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

    set_repeat_info(&wayland->input, 40, 400);
    wayland->input.last_code = 0xDEADBEEF;
    return true;
}

/* vim: set ts=8 sw=4 tw=0 :*/
