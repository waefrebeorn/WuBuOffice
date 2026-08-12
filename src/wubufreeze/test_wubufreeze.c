#include "wubufreeze.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubufreeze f;
    CK(wubufreeze_init(&f, 1, 1) == 0, "init");
    CK(wubufreeze_frozen_rows(&f) == 1 && wubufreeze_frozen_cols(&f) == 1, "frozen counts");

    CK(wubufreeze_visible_row(&f, 0) == -1, "row0 frozen");
    CK(wubufreeze_visible_col(&f, 0) == -1, "col0 frozen");

    /* scroll to row 5 col 3: logical row 5 is visible at offset 3 */
    wubufreeze_scroll(&f, 5, 3);
    CK(wubufreeze_visible_row(&f, 5) == 5 - 1 - 5, "row5 offset after scroll");
    CK(wubufreeze_visible_row(&f, 3) == 3 - 1 - 5, "row3 is scrollable (not frozen)");
    CK(wubufreeze_visible_col(&f, 4) == 4 - 1 - 3, "col4 offset");

    /* no freeze */
    wubufreeze g;
    wubufreeze_init(&g, 0, 0);
    CK(wubufreeze_visible_row(&g, 2) == 2, "no freeze passthrough");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubufreeze (frozen header rows/cols, scroll origin, visibility map)\n");
    return 0;
}
