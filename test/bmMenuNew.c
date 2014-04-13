#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <bemenu.h>

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    // TEST: Instance bmMenu with all possible draw modes.
    {
        bmDrawMode i;
        for (i = 0; i < BM_DRAW_MODE_LAST; ++i) {
            if (i == BM_DRAW_MODE_CURSES && !isatty(STDIN_FILENO)) {
                printf("Skipping test for mode BM_DRAW_MODE_CURSES, as not running on terminal.\n");
                continue;
            }
            bmMenu *menu = bmMenuNew(i);
            assert(menu);
            bmMenuRender(menu);
            bmMenuFree(menu);
        }
    }

    return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=4 tw=0 :*/
