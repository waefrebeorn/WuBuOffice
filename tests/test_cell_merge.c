/* test_cell_merge.c -- verify cell-merge (write + read-back round trip).
 *
 * wubucell_merge() records a rectangular merged region; cell_render_sheet()
 * emits a <mergeCells> block with A1:B2-style refs; cell_read.c parses the
 * mergeCell entries back. This test proves the whole loop: assemble a book
 * with a merge, read it back, and confirm the region survives byte-for-byte
 * in both directions. Plus the count/get accessors.
 *
 * CHECK-based so it fails loudly (not silent) even under -DNDEBUG. */

#include "cell_internal.h"
#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

int main(void) {
    const char *path = "/tmp/test_cell_merge.xlsx";

    /* --- write side --- */
    wubucell_book *b = wubucell_create();
    int sh = wubucell_sheet(b, "Sheet1");
    wubucell_cell_s(b, sh, 1, 1, "Big Header");   /* A1 holds the value */
    wubucell_cell_n(b, sh, 1, 2, 42);             /* A2 */
    wubucell_merge(b, sh, 1, 1, 3, 2);            /* merge A1:C2 */

    /* accessors on the live book */
    CHECK(wubucell_merge_count(b, sh) == 1, "merge_count == 1 (write)");
    int c0, r0, c1, r1;
    CHECK(wubucell_merge_get(b, sh, 0, &c0, &r0, &c1, &r1) == 0, "merge_get ok");
    CHECK(c0 == 1 && r0 == 1 && c1 == 3 && r1 == 2, "merge rect A1:C2");
    CHECK(wubucell_merge_get(b, sh, 5, &c0, &r0, &c1, &r1) == -1, "merge_get oob -> -1");
    CHECK(wubucell_merge_count(b, 99) == -1, "merge_count bad sheet -> -1");

    if (wubucell_assemble(b, path) != 0) { printf("FAIL assemble\n"); wubucell_free(b); return 1; }
    wubucell_free(b);

    /* --- read side --- */
    wubucell_book *r = NULL;
    if (wubucell_read(path, &r) != 0) { printf("FAIL read\n"); return 1; }
    CHECK(wubucell_merge_count(r, sh) == 1, "merge_count == 1 (read)");
    CHECK(wubucell_merge_get(r, sh, 0, &c0, &r0, &c1, &r1) == 0, "merge_get ok (read)");
    CHECK(c0 == 1 && r0 == 1 && c1 == 3 && r1 == 2, "merge rect survives round trip A1:C2");
    /* the surviving value lives in the top-left cell */
    sheet_t *s = cell_book_sheet(r, sh);
    CHECK(s != NULL, "sheet present");
    int found = 0;
    for (size_t i = 0; s && i < s->n; i++)
        if (s->cells[i].col == 1 && s->cells[i].row == 1)
            { found = 1; CHECK(strcmp(s->cells[i].text, "Big Header") == 0, "A1 value preserved"); }
    CHECK(found, "A1 present");
    wubucell_free(r);

    printf("cell_merge: XLSX merge write+read round trip PASSED\n");
    return 0;
}
