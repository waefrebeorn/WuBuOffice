#include "cell.h"
#include <stdio.h>
#include <stdlib.h>

int wubucell_main(int argc, char **argv) {
    const char *outpath = (argc > 1) ? argv[1] : "WuBuOffice.xlsx";
    wubucell_book *b = wubucell_create();
    wubucell_use_shared_strings(b, 1);   /* real Excel default: shared strings */

    int s1 = wubucell_sheet(b, "Numbers");
    wubucell_cell_s(b, s1, 1, 1, "Layer");
    wubucell_cell_s(b, s1, 2, 1, "Year");
    wubucell_cell_s(b, s1, 3, 1, "Status");
    wubucell_cell_s(b, s1, 1, 2, "wubuzip");
    wubucell_cell_n(b, s1, 2, 2, 2026);
    wubucell_cell_s(b, s1, 3, 2, "working");
    wubucell_cell_s(b, s1, 1, 3, "wubucell");
    wubucell_cell_n(b, s1, 2, 3, 2026);
    wubucell_cell_s(b, s1, 3, 3, "fresh");

    int s2 = wubucell_sheet(b, "Math");
    wubucell_cell_s(b, s2, 1, 1, "Expr");
    wubucell_cell_s(b, s2, 2, 1, "Result");
    wubucell_cell_s(b, s2, 1, 2, "2+2");
    wubucell_cell_n(b, s2, 2, 2, 4);
    /* real formula: sum of the two Year cells above, with a cached value */
    wubucell_cell_f(b, s2, 2, 3, "B2+B3", 4052);

    if (wubucell_assemble(b, outpath) != 0) {
        fprintf(stderr, "wubucell: assemble failed\n");
        wubucell_free(b);
        return 1;
    }
    wubucell_free(b);
    fprintf(stderr, "wubucell: wrote %s (shared strings + formula)\n", outpath);
    return 0;
}
