#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <bemenu.h>

int
main(int argc, char **argv)
{
    (void)argc, (void)argv;

    setenv("BEMENU_RENDERERS", "../renderers", true);

    if (!bm_init())
        return EXIT_FAILURE;

    // TEST: Instance bmMenu with all possible draw modes.
    {
        uint32_t count;
        const struct bm_renderer **renderers = bm_get_renderers(&count);
        for (int32_t i = 0; i < count; ++i) {
            if (!strcmp(bm_renderer_get_name(renderers[i]), "curses") && !isatty(STDIN_FILENO)) {
                printf("Skipping test for mode BM_DRAW_MODE_CURSES, as not running on terminal.\n");
                continue;
            }
            struct bm_menu *menu = bm_menu_new(bm_renderer_get_name(renderers[i]));
            assert(menu);
            bm_menu_render(menu);
            bm_menu_free(menu);
        }
    }

    return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=4 tw=0 :*/
