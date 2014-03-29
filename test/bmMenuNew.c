#include <stdlib.h>
#include <bemenu.h>
#include <assert.h>

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    // TEST: Instance bmMenu with all possible draw modes.
    {
        bmDrawMode i;
        for (i = 0; i < BM_DRAW_MODE_LAST; ++i) {
            bmMenu *menu = bmMenuNew(i);
            assert(menu != NULL);
            bmMenuRender(menu);
            bmMenuFree(menu);
        }
    }

    return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=4 tw=0 :*/
