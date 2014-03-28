/**
 * @file client.c
 *
 * Sample client using the libbemenu.
 * Also usable as dmenu replacement.
 */

#include <stdlib.h>
#include <bemenu.h>

/**
 * Main method
 *
 * This function gives and takes the life of our program.
 *
 * @param argc Number of arguments from command line
 * @param argv Pointer to array of the arguments
 * @return exit status of the program
 */
int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    bmMenu *menu = bmMenuNew(BM_DRAW_MODE_NONE);

    if (!menu)
        return EXIT_FAILURE;

    bmMenuRender(menu);

    bmMenuFree(menu);

    return EXIT_SUCCESS;
}

/* vim: set ts=8 sw=4 tw=0 :*/
