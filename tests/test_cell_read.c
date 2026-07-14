#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_read.h"
#include "../apps/wubucell/cell_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* xlsx read path: write a book (shared strings + a SUM formula with cached
 * value), read it back into a fresh wubucell_book, confirm the string/number
 * cells survive, and that the formula engine recomputes the cached value. */

static sheet_t *find_sheet(wubucell_book *b, int idx) {
    return cell_book_sheet(b, idx);
}
static const char *cell_str(sheet_t *s, int col, int row) {
    for (size_t i = 0; i < s->n; i++)
        if (s->cells[i].col == col && s->cells[i].row == row)
            return s->cells[i].text;
    return NULL;
}
static double cell_num(sheet_t *s, int col, int row) {
    for (size_t i = 0; i < s->n; i++)
        if (s->cells[i].col == col && s->cells[i].row == row)
            return s->cells[i].num;
    return -1e300;
}

int main(void) {
    const char *path = "/tmp/test_cell_read.xlsx";
    wubucell_book *b = wubucell_create();
    int sh = wubucell_sheet(b, "Data");   /* 1-based sheet index */
    /* 1-based columns: col 1 = A, col 2 = B */
    wubucell_cell_s(b, sh, 1, 1, "Item");
    wubucell_cell_s(b, sh, 2, 1, "Cost");
    wubucell_cell_s(b, sh, 1, 2, "Engine");
    wubucell_cell_n(b, sh, 2, 2, 1200.5);
    wubucell_cell_s(b, sh, 1, 3, "Docs");
    wubucell_cell_n(b, sh, 2, 3, 320);
    wubucell_cell_f(b, sh, 2, 4, "SUM(B2:B3)", 1520.5);
    if (wubucell_assemble(b, path) != 0) { printf("FAIL assemble\n"); wubucell_free(b); return 1; }
    wubucell_free(b);

    wubucell_book *r = NULL;
    if (wubucell_read(path, &r) != 0) { printf("FAIL read\n"); return 1; }

    int ok = 1;
    sheet_t *s = find_sheet(r, sh);   /* 1-based */
    if (!s) { printf("FAIL no sheet\n"); ok = 0; }
    else {
        if (strcmp(cell_str(s, 1, 1) ? cell_str(s, 1, 1) : "", "Item") != 0) { printf("FAIL A1=%s\n", cell_str(s,1,1)); ok = 0; }
        if (strcmp(cell_str(s, 1, 2) ? cell_str(s, 1, 2) : "", "Engine") != 0) { printf("FAIL A2=%s\n", cell_str(s,1,2)); ok = 0; }
        if (cell_num(s, 2, 2) != 1200.5) { printf("FAIL B2=%g\n", cell_num(s,2,2)); ok = 0; }
        if (cell_num(s, 2, 3) != 320.0) { printf("FAIL B3=%g\n", cell_num(s,2,3)); ok = 0; }
        /* formula cell B4: cached value should be 1520.5 */
        int found = 0;
        for (size_t i = 0; i < s->n; i++) {
            if (s->cells[i].col == 2 && s->cells[i].row == 4) {
                found = 1;
                if (s->cells[i].kind != C_FORM) { printf("FAIL B4 not formula\n"); ok = 0; }
                if (s->cells[i].cached != 1520.5) { printf("FAIL B4 cached=%g\n", s->cells[i].cached); ok = 0; }
            }
        }
        if (!found) { printf("FAIL B4 missing\n"); ok = 0; }
    }

    /* recalc on load: recompute the formula and confirm it matches the cache */
    if (ok) {
        cell_eval_all(r);
        sheet_t *s2 = find_sheet(r, sh);
        for (size_t i = 0; i < s2->n; i++) {
            if (s2->cells[i].col == 2 && s2->cells[i].row == 4) {
                if (s2->cells[i].cached != 1520.5) { printf("FAIL recalc=%g\n", s2->cells[i].cached); ok = 0; }
            }
        }
    }

    wubucell_free(r);
    if (!ok) { printf("XLSX READ ROUNDTRIP FAILED\n"); return 1; }
    printf("XLSX READ ROUNDTRIP PASSED (shared strings + formula recalc)\n");
    return 0;
}
