/* test_odf -- OpenDocument (odt/ods/odp) write + read round-trips.
 *
 * For each format: build a model, write the .od? file, read it back, assert the
 * content survives. Proves ODF supremacy over the same three universal models.
 * The container shape (mimetype first + manifest) is checked separately by the
 * conformance oracle (validate.py) when run under ctest. */

#include "../apps/wubuodf/odf.h"
#include "../apps/wubuedit/docmodel.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a small dm_doc by hand (heading + bold para + 2x2 table). */
static void build_doc(dm_doc *d) {
    memset(d, 0, sizeof *d);
    d->cap = 8; d->blocks = calloc(d->cap, sizeof *d->blocks);
    /* heading */
    dm_block *h = &d->blocks[d->n++];
    h->kind = DM_BLOCK_PARA; h->para.style = strdup("Heading1"); h->para.text = strdup("ODF Title");
    /* bold para */
    dm_block *p = &d->blocks[d->n++];
    p->kind = DM_BLOCK_PARA; p->para.bold = 1; p->para.text = strdup("Bold body.");
    /* table */
    dm_block *t = &d->blocks[d->n++];
    t->kind = DM_BLOCK_TABLE; t->table.rows = 2; t->table.cols = 2;
    t->table.cells = calloc(4, sizeof(dm_para *));
    const char *vals[4] = {"A", "B", "1", "2"};
    for (int i = 0; i < 4; i++) { dm_para *c = calloc(1, sizeof *c); c->text = strdup(vals[i]); t->table.cells[i] = c; }
}

static int test_odt(void) {
    dm_doc d; build_doc(&d);
    if (wubuodf_write_odt(&d, "/tmp/test.odt") != 0) { printf("FAIL write_odt\n"); return 1; }
    wubuedit_docmodel_free(&d);

    dm_doc r;
    if (wubuodf_read_odt("/tmp/test.odt", &r) != 0) { printf("FAIL read_odt\n"); return 1; }
    int rc = 0, saw_h = 0, saw_bold = 0, saw_table = 0;
    for (size_t i = 0; i < r.n; i++) {
        dm_block *b = &r.blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            if (b->para.style && strcmp(b->para.style, "Heading1") == 0 && b->para.text && strcmp(b->para.text, "ODF Title") == 0) saw_h = 1;
            if (b->para.bold && b->para.text && strcmp(b->para.text, "Bold body.") == 0) saw_bold = 1;
        } else if (b->kind == DM_BLOCK_TABLE) {
            if (b->table.rows == 2 && b->table.cols == 2) {
                dm_para *c0 = b->table.cells[0], *c3 = b->table.cells[3];
                if (c0 && c0->text && strcmp(c0->text, "A") == 0 && c3 && c3->text && strcmp(c3->text, "2") == 0) saw_table = 1;
            }
        }
    }
    if (!saw_h) { printf("FAIL odt heading\n"); rc = 1; }
    if (!saw_bold) { printf("FAIL odt bold\n"); rc = 1; }
    if (!saw_table) { printf("FAIL odt table\n"); rc = 1; }
    wubuedit_docmodel_free(&r);
    if (!rc) printf("ODT OK\n");
    return rc;
}

static int test_ods(void) {
    wubucell_book *b = wubucell_create();
    int sh = wubucell_sheet(b, "Data");
    wubucell_cell_s(b, sh, 1, 1, "Item");
    wubucell_cell_n(b, sh, 2, 1, 1200.5);
    wubucell_cell_f(b, sh, 2, 2, "B1*2", 2401);
    int sh2 = wubucell_sheet(b, "Second");
    wubucell_cell_s(b, sh2, 1, 1, "Hi");
    if (wubuodf_write_ods(b, "/tmp/test.ods") != 0) { printf("FAIL write_ods\n"); wubucell_free(b); return 1; }
    wubucell_free(b);

    wubucell_book *r = NULL;
    if (wubuodf_read_ods("/tmp/test.ods", &r) != 0) { printf("FAIL read_ods\n"); return 1; }
    int rc = 0;
    if (wubucell_sheet_count(r) != 2) { printf("FAIL ods sheets=%d\n", wubucell_sheet_count(r)); rc = 1; }
    wubucell_ckind k; const char *t = NULL; double n = 0, ca = 0;
    if (wubucell_get(r, 1, 1, 1, &k, &t, &n, &ca) != 0 || k != WUBUCELL_STR || strcmp(t, "Item")) { printf("FAIL ods A1\n"); rc = 1; }
    if (wubucell_get(r, 1, 2, 1, &k, &t, &n, &ca) != 0 || k != WUBUCELL_NUM || n != 1200.5) { printf("FAIL ods B1=%g\n", n); rc = 1; }
    if (wubucell_get(r, 1, 2, 2, &k, &t, &n, &ca) != 0 || k != WUBUCELL_FORM) { printf("FAIL ods B2 kind\n"); rc = 1; }
    if (wubucell_get(r, 2, 1, 1, &k, &t, &n, &ca) != 0 || k != WUBUCELL_STR || strcmp(t, "Hi")) { printf("FAIL ods sheet2\n"); rc = 1; }
    wubucell_free(r);
    if (!rc) printf("ODS OK\n");
    return rc;
}

static int test_odp(void) {
    wubushow_pres *p = wubushow_create();
    wubushow_slide(p, "Slide One", "Alpha\nBeta");
    wubushow_slide(p, "Slide Two", "Only one.");
    if (wubuodf_write_odp(p, "/tmp/test.odp") != 0) { printf("FAIL write_odp\n"); wubushow_free(p); return 1; }
    wubushow_free(p);

    wubushow_pres *r = NULL;
    if (wubuodf_read_odp("/tmp/test.odp", &r) != 0) { printf("FAIL read_odp\n"); return 1; }
    int rc = 0;
    if (wubushow_slide_count(r) != 2) { printf("FAIL odp slides=%d\n", wubushow_slide_count(r)); rc = 1; }
    else {
        const char *t0 = NULL, *b0 = NULL, *t1 = NULL, *b1 = NULL;
        wubushow_slide_get(r, 0, &t0, &b0);
        wubushow_slide_get(r, 1, &t1, &b1);
        if (!t0 || strcmp(t0, "Slide One")) { printf("FAIL odp s1 title=%s\n", t0 ? t0 : "?"); rc = 1; }
        if (!b0 || strcmp(b0, "Alpha\nBeta")) { printf("FAIL odp s1 body=%s\n", b0 ? b0 : "?"); rc = 1; }
        if (!t1 || strcmp(t1, "Slide Two")) { printf("FAIL odp s2 title\n"); rc = 1; }
        if (!b1 || strcmp(b1, "Only one.")) { printf("FAIL odp s2 body=%s\n", b1 ? b1 : "?"); rc = 1; }
    }
    wubushow_free(r);
    if (!rc) printf("ODP OK\n");
    return rc;
}

/* Foreign-file leg: produce ODF with odfpy, read it back with our C readers.
 * Also drop wubu-written files where the odfpy validate step can find them.
 * SKIPs cleanly (returns 0) when odfpy is unavailable. */
static int test_foreign_odf(void) {
    const char *py = getenv("WUBU_CONFORMANCE_PYTHON"); if (!py) py = "python3";
    const char *script = "tests/conformance/odf_oracle.py";
    #ifdef ODF_ORACLE
    script = ODF_ORACLE;
    #endif
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s %s produce /tmp/odf_foreign 2>/dev/null", py, script);
    int rc = system(cmd);
    int code = (rc == -1) ? -1 : (rc >> 8) & 0xff;
    if (code == 2) { printf("FOREIGN ODF SKIPPED (odfpy unavailable)\n"); return 0; }
    if (code != 0) { printf("FAIL odfpy produce (rc=%d)\n", code); return 1; }

    int fail = 0;
    /* .odt produced by odfpy */
    dm_doc r;
    if (wubuodf_read_odt("/tmp/odf_foreign/foreign.odt", &r) != 0) { printf("FAIL read foreign odt\n"); return 1; }
    int saw = 0;
    for (size_t i = 0; i < r.n; i++)
        if (r.blocks[i].kind == DM_BLOCK_PARA && r.blocks[i].para.text &&
            strcmp(r.blocks[i].para.text, "Foreign Heading") == 0) saw = 1;
    if (!saw) { printf("FAIL foreign odt heading\n"); fail = 1; }
    wubuedit_docmodel_free(&r);

    /* .ods produced by odfpy */
    wubucell_book *b = NULL;
    if (wubuodf_read_ods("/tmp/odf_foreign/foreign.ods", &b) != 0) { printf("FAIL read foreign ods\n"); return 1; }
    wubucell_ckind k; const char *t = NULL; double n = 0, ca = 0;
    if (wubucell_get(b, 1, 1, 1, &k, &t, &n, &ca) != 0 || !t || strcmp(t, "Name")) { printf("FAIL foreign ods A1\n"); fail = 1; }
    if (wubucell_get(b, 1, 2, 1, &k, &t, &n, &ca) != 0 || k != WUBUCELL_NUM || n != 99.5) { printf("FAIL foreign ods B1=%g\n", n); fail = 1; }
    wubucell_free(b);

    /* .odp produced by odfpy (best-effort: odfpy's presentation API is finicky,
     * so only assert when the producer actually emitted the file). */
    FILE *odp = fopen("/tmp/odf_foreign/foreign.odp", "rb");
    if (odp) {
        fclose(odp);
        wubushow_pres *p = NULL;
        if (wubuodf_read_odp("/tmp/odf_foreign/foreign.odp", &p) != 0) { printf("FAIL read foreign odp\n"); return 1; }
        if (wubushow_slide_count(p) < 1) { printf("FAIL foreign odp slides\n"); fail = 1; }
        else { const char *tt = NULL, *bb = NULL; wubushow_slide_get(p, 0, &tt, &bb);
               if (!tt || strcmp(tt, "Foreign Slide")) { printf("FAIL foreign odp title=%s\n", tt ? tt : "?"); fail = 1; } }
        wubushow_free(p);
    }

    if (!fail) printf("FOREIGN ODF OK (odfpy produced; WuBuOffice consumed)\n");
    return fail;
}

int main(void) {
    int rc = 0;
    rc |= test_odt();
    rc |= test_ods();
    rc |= test_odp();
    rc |= test_foreign_odf();
    if (rc) { printf("ODF TEST FAILED\n"); return 1; }
    printf("ODF TEST PASSED (odt/ods/odp write+read + foreign odfpy interop)\n");
    return 0;
}
