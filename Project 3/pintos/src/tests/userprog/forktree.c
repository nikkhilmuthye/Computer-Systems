/* -*- c -*- */

#include <syscall.h>
#include "tests/lib.h"

int 
main (void)
{
    int depth = 3;
    int total = 0;

    msg("begin");
    while (depth > 0) {
        total += depth;
        if (fork() == -1) {
            fail("fail %d", depth);
        }
        depth--;
    }
    msg("%d\n", total);
    return 0;
}

