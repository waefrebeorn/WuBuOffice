/* test_cell_cycle.c — regression guard for the circular-reference crash
 * in the spreadsheet evaluator (apps/wubucell/cell_eval.c).
 *
 * Previously cell_eval_all() had DEAD cycle-detection code: the
 * visit[k]==1 -> #CYCLE! branch in the outer loop was unreachable
 * (visit[k] is already 2 by the time the loop re-checks it), and the
 * recursive resolver cell_br_resolve() never bailed on re-entry. A
 * workbook with A1=B1+1, B1=A1+1 therefore recursed forever and
 * STACK-OVERFLOWED the app on assembly (the user types =A1 in A1).
 *
 * Now cell_br_resolve() returns #CYCLE! on re-entry, and every cycle
 * member (including transitive dependents) is marked #CYCLE!. This
 * test asserts both the crash is gone and the error surfaces.
 *
 * CHECK (not assert) so failures surface under -DNDEBUG. */

#include "cell_internal.h"
#include "cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

static wubucell_book *make_cycle_book(void) {
    wubucell_book *b = calloc(1, sizeof *b);
    b->sheets = calloc(1, sizeof *b->sheets);
    b->n = 1;
    b->sheets[0].name = strdup("Sheet1");
    b->sheets[0].cells = calloc(4, sizeof(cell_t));
    b->sheets[0].n = 3;
    b->sheets[0].cells[0].col = 1; b->sheets[0].cells[0].row = 1;
    b->sheets[0].cells[0].kind = C_FORM; b->sheets[0].cells[0].formula = strdup("B1+1");
    b->sheets[0].cells[1].col = 2; b->sheets[0].cells[1].row = 1;
    b->sheets[0].cells[1].kind = C_FORM; b->sheets[0].cells[1].formula = strdup("A1+1");
    b->sheets[0].cells[2].col = 3; b->sheets[0].cells[2].row = 1;
    b->sheets[0].cells[2].kind = C_FORM; b->sheets[0].cells[2].formula = strdup("A1*2");
    return b;
}

static void free_book(wubucell_book *b) {
    for (size_t i = 0; i < b->n; i++) {
        free(b->sheets[i].name);
        for (size_t j = 0; j < b->sheets[i].n; j++) {
            free(b->sheets[i].cells[j].text);
            free(b->sheets[i].cells[j].formula);
        }
        free(b->sheets[i].cells);
    }
    free(b->sheets);
    free(b);
}

int main(void) {
    wubucell_book *b = make_cycle_book();
    cell_eval_all(b);   /* must NOT crash (was a stack overflow) */

    /* all three cells sit in / depend on the cycle -> #CYCLE! */
    CHECK(b->sheets[0].cells[0].kind == C_STR, "A1 is string");
    CHECK(b->sheets[0].cells[0].text && strcmp(b->sheets[0].cells[0].text, "#CYCLE!") == 0, "A1 = #CYCLE!");
    CHECK(b->sheets[0].cells[1].kind == C_STR, "B1 is string");
    CHECK(b->sheets[0].cells[1].text && strcmp(b->sheets[0].cells[1].text, "#CYCLE!") == 0, "B1 = #CYCLE!");
    CHECK(b->sheets[0].cells[2].kind == C_STR, "C1 is string");
    CHECK(b->sheets[0].cells[2].text && strcmp(b->sheets[0].cells[2].text, "#CYCLE!") == 0, "C1 = #CYCLE! (transitive)");

    free_book(b);

    /* sanity: a NON-cyclic formula still computes correctly */
    wubucell_book *b2 = calloc(1, sizeof *b2);
    b2->sheets = calloc(1, sizeof *b2->sheets);
    b2->n = 1;
    b2->sheets[0].name = strdup("S");
    b2->sheets[0].cells = calloc(3, sizeof(cell_t));
    b2->sheets[0].n = 3;
    b2->sheets[0].cells[0].col = 1; b2->sheets[0].cells[0].row = 1;
    b2->sheets[0].cells[0].kind = C_NUM; b2->sheets[0].cells[0].num = 10;
    b2->sheets[0].cells[1].col = 2; b2->sheets[0].cells[1].row = 1;
    b2->sheets[0].cells[1].kind = C_NUM; b2->sheets[0].cells[1].num = 5;
    b2->sheets[0].cells[2].col = 3; b2->sheets[0].cells[2].row = 1;
    b2->sheets[0].cells[2].kind = C_FORM; b2->sheets[0].cells[2].formula = strdup("A1+B1");
    cell_eval_all(b2);
    CHECK(b2->sheets[0].cells[2].kind == C_FORM, "C1 remains a formula cell");
    CHECK(b2->sheets[0].cells[2].cached == 15.0, "C1 = A1+B1 = 15 (cached)");
    free_book(b2);

    /* Integration: the real crash vector is wubucell_assemble() on a workbook
     * containing a circular reference. It calls cell_eval_all() first; before
     * the fix this stack-overflowed. Assemble must succeed and produce a valid
     * .xlsx (ZIP). */
    {
        wubucell_book *b3 = wubucell_create();
        int sh = wubucell_sheet(b3, "Sheet1");
        wubucell_cell_n(b3, sh, 1, 1, 10);                 /* A1 = 10 */
        wubucell_cell_n(b3, sh, 2, 1, 5);                  /* B1 = 5 */
        wubucell_cell_f(b3, sh, 3, 1, "A1+B1", 0);         /* C1 = A1+B1 (normal) */
        wubucell_cell_f(b3, sh, 4, 1, "E1+1", 0);          /* D1 = E1+1 */
        wubucell_cell_f(b3, sh, 5, 1, "D1+1", 0);          /* E1 = D1+1 (cycle D1<->E1) */
        int rc = wubucell_assemble(b3, "/tmp/test_cycle_out.xlsx");
        CHECK(rc == 0, "assemble cyclic workbook succeeds (no stack overflow)");
        /* valid ZIP ends with the End-Of-Central-Directory signature 0x06054b50
         * (little-endian "PK\x05\x06") somewhere in the last 22 bytes. */
        FILE *z = fopen("/tmp/test_cycle_out.xlsx", "rb");
        int ok_zip = 0;
        if (z) {
            fseek(z, 0, SEEK_END); long sz = ftell(z); fseek(z, 0, SEEK_SET);
            unsigned char *buf = malloc((size_t)(sz > 0 ? sz : 1));
            if (buf && fread(buf, 1, (size_t)sz, z) == (size_t)sz) {
                for (long i = sz - 22; i >= 0; i--)
                    if (buf[i]==0x50 && buf[i+1]==0x4b && buf[i+2]==0x05 && buf[i+3]==0x06) { ok_zip = 1; break; }
            }
            free(buf); fclose(z);
        }
        CHECK(ok_zip, "assembled file is a valid ZIP (.xlsx)");
        wubucell_free(b3);
    }

    printf("cell_cycle: circular-reference crash fixed + #CYCLE! surfaces + normal eval OK\n");
    return 0;
}
