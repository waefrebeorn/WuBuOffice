#include "cell.h"
#include "style.h"
#include <stdio.h>
#include <stdlib.h>

int wubucell_main(int argc, char **argv) {
    const char *outpath = (argc > 1) ? argv[1] : "WuBuOffice.xlsx";
    wubucell_book *b = wubucell_create();
    wubucell_use_shared_strings(b, 1);

    struct wubucell_style *st = wubucell_styles(b);
    int f_title = wubucell_style_font(st, "Calibri", 14, 1, 0, "FF1F4E78");
    int f_bold  = wubucell_style_font(st, "Calibri", 11, 1, 0, NULL);
    int fill_hdr = wubucell_style_fill(st, "FFD9E1F2");
    int border   = wubucell_style_border(st, "thin", "FFBFBFBF");
    int money    = wubucell_style_numfmt(st, "\"$\"#,##0.00");

    int s_title = wubucell_style_cell(st, f_title, 0, 0, 0, "left");
    int s_hdr   = wubucell_style_cell(st, f_bold, fill_hdr, border, 0, "center");
    int s_money = wubucell_style_cell(st, 0, 0, border, money, "right");

    int s = wubucell_sheet(b, "Budget");
    wubucell_cell_sx(b, s, 1, 1, "WuBu Office Budget 2026", s_title);
    wubucell_cell_sx(b, s, 1, 2, "Item", s_hdr);
    wubucell_cell_sx(b, s, 2, 2, "Cost", s_hdr);
    wubucell_cell_sx(b, s, 1, 3, "wubuzip", s_hdr);
    wubucell_cell_nx(b, s, 2, 3, 0.0, s_money);
    wubucell_cell_sx(b, s, 1, 4, "wubucell", s_hdr);
    wubucell_cell_nx(b, s, 2, 4, 1200.50, s_money);
    wubucell_cell_sx(b, s, 1, 5, "wubushow", s_hdr);
    wubucell_cell_nx(b, s, 2, 5, 320.00, s_money);
    wubucell_cell_sx(b, s, 1, 6, "Total", s_hdr);
    wubucell_cell_fx(b, s, 2, 6, "SUM(B3:B5)", 1520.50, s_money);

    /* bar chart of the costs */
    wubucell_chart(b, s, "Cost by Layer", "Budget!A3:A5", "Budget!B3:B5");

    if (wubucell_assemble(b, outpath) != 0) {
        fprintf(stderr, "wubucell: assemble failed\n");
        wubucell_free(b);
        return 1;
    }
    wubucell_free(b);
    fprintf(stderr, "wubucell: wrote %s (styles + chart)\n", outpath);
    return 0;
}
