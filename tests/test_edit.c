#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuedit/edit.h"
#include "../apps/wubuedit/docmodel.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_read.h"
#include "../apps/wubushow/show.h"
#include "../apps/wubushow/show_read.h"
#include "../src/wubuoxml/reader.h"
#include "../src/wubuoxml/docx_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Structure-preserving round-trip for docx/xlsx/pptx: build a doc with a
 * Heading1, a bold paragraph and a 2x2 table; round-trip through wubuedit;
 * then re-parse the OUTPUT and confirm the structure survived. Sanitizer-clean.
 *
 * The xlsx/pptx legs prove the new multi-format edit path (wubucell_read +
 * wubushow_read) re-emits a lossless document through our own writers, and the
 * independent conformance oracle (openpyxl/python-pptx) is the real fidelity
 * gate. */

static uint8_t *slurp(const char *p, size_t *o) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(s ? s : 1);
    if (fread(d, 1, s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f); *o = s; return d;
}

static int check_docx(void) {
    const char *src = "/tmp/test_edit_docx_src.docx";
    const char *dst = "/tmp/test_edit_docx_dst.docx";
    wubuword_doc *d = wubuword_create();
    wubuword_para(d, "Heading1", 0, "The Title");
    wubuword_para(d, NULL, 1, "Bold body line.");
    wubuword_table_begin(d);
    wubuword_row(d); wubuword_cell(d, 1, "A1"); wubuword_cell(d, 0, "B1");
    wubuword_row(d); wubuword_cell(d, 0, "A2"); wubuword_cell(d, 1, "B2");
    wubuword_table_end(d);
    size_t len = 0; char *doc = wubuword_render(d, &len); wubuword_free(d);
    if (wubuword_assemble(src, doc, len) != 0) { free(doc); printf("FAIL docx src\n"); return 1; }
    free(doc);
    if (wubuedit_roundtrip(src, dst) != 0) { printf("FAIL docx roundtrip\n"); return 1; }

    size_t sz = 0; uint8_t *data = slurp(dst, &sz);
    wubuoxml_package pkg;
    if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); printf("FAIL docx read dst\n"); return 1; }
    const wubuoxml_part *docpart = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!docpart) { wubuoxml_free(&pkg); free(data); printf("FAIL docx no document.xml\n"); return 1; }
    dm_doc m;
    if (wubuedit_docmodel_parse(docpart->bytes, docpart->len, &m) != 0) {
        wubuoxml_free(&pkg); free(data); printf("FAIL docx reparse\n"); return 1;
    }
    int saw_heading = 0, saw_bold = 0, saw_table = 0, table_ok = 0;
    for (size_t i = 0; i < m.n; i++) {
        dm_block *b = &m.blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            if (b->para.style && strcmp(b->para.style, "Heading1") == 0) saw_heading = 1;
            if (b->para.bold) saw_bold = 1;
        } else if (b->kind == DM_BLOCK_TABLE) {
            saw_table = 1;
            if (b->table.rows == 2 && b->table.cols == 2) table_ok = 1;
        }
    }
    wubuedit_docmodel_free(&m); wubuoxml_free(&pkg); free(data);
    int ok = saw_heading && saw_bold && saw_table && table_ok;
    if (!ok) printf("FAIL docx structure: heading=%d bold=%d table=%d(2x2=%d)\n", saw_heading, saw_bold, saw_table, table_ok);
    return ok ? 0 : 1;
}

static int check_xlsx(void) {
    const char *src = "/tmp/test_edit_xlsx_src.xlsx";
    const char *dst = "/tmp/test_edit_xlsx_dst.xlsx";
    wubucell_book *b = wubucell_create();
    int sh = wubucell_sheet(b, "Data");
    wubucell_cell_s(b, sh, 1, 1, "Item");
    wubucell_cell_n(b, sh, 2, 1, 9.5);
    wubucell_cell_f(b, sh, 2, 2, "SUM(B1:B1)", 9.5);
    if (wubucell_assemble(b, src) != 0) { wubucell_free(b); printf("FAIL xlsx src\n"); return 1; }
    wubucell_free(b);
    if (wubuedit_roundtrip(src, dst) != 0) { printf("FAIL xlsx roundtrip\n"); return 1; }

    wubucell_book *r = NULL;
    if (wubucell_read(dst, &r) != 0) { printf("FAIL xlsx read dst\n"); return 1; }
    int ok = 1;
    const char *a1 = NULL; double b1 = -1, b2 = -1; wubucell_ckind k2 = WUBUCELL_NUM;
    wubucell_get(r, sh, 1, 1, NULL, &a1, NULL, NULL);
    wubucell_get(r, sh, 2, 1, NULL, NULL, &b1, NULL);
    wubucell_get(r, sh, 2, 2, &k2, NULL, NULL, &b2);
    if (!a1 || strcmp(a1, "Item") != 0) { ok = 0; printf("FAIL xlsx A1=%s\n", a1?a1:"<null>"); }
    if (b1 != 9.5) { ok = 0; printf("FAIL xlsx B1=%g\n", b1); }
    if (k2 != WUBUCELL_FORM) { ok = 0; printf("FAIL xlsx B2 kind=%d\n", (int)k2); }
    if (b2 != 9.5) { ok = 0; printf("FAIL xlsx B2 cached=%g\n", b2); }
    wubucell_free(r);
    return ok ? 0 : 1;
}

static int check_pptx(void) {
    const char *src = "/tmp/test_edit_pptx_src.pptx";
    const char *dst = "/tmp/test_edit_pptx_dst.pptx";
    wubushow_pres *p = wubushow_create();
    wubushow_slide(p, "Title One", "Bullet A.\nBullet B.");
    wubushow_slide(p, "Title Two", "Only bullet.");
    if (wubushow_assemble(p, src) != 0) { wubushow_free(p); printf("FAIL pptx src\n"); return 1; }
    wubushow_free(p);
    if (wubuedit_roundtrip(src, dst) != 0) { printf("FAIL pptx roundtrip\n"); return 1; }

    wubushow_pres *r = NULL;
    if (wubushow_read(dst, &r) != 0) { printf("FAIL pptx read dst\n"); return 1; }
    int ok = 1;
    if (wubushow_slide_count(r) != 2) { ok = 0; printf("FAIL pptx slide count=%d\n", wubushow_slide_count(r)); }
    else {
        const char *t0 = NULL, *b0 = NULL, *t1 = NULL, *b1 = NULL;
        wubushow_slide_get(r, 0, &t0, &b0);
        wubushow_slide_get(r, 1, &t1, &b1);
        if (!t0 || strcmp(t0, "Title One") != 0) { ok = 0; printf("FAIL pptx s1 title=%s\n", t0?t0:"<null>"); }
        if (!b0 || strcmp(b0, "Bullet A.\nBullet B.") != 0) { ok = 0; printf("FAIL pptx s1 body=%s\n", b0?b0:"<null>"); }
        if (!t1 || strcmp(t1, "Title Two") != 0) { ok = 0; printf("FAIL pptx s2 title=%s\n", t1?t1:"<null>"); }
        if (!b1 || strcmp(b1, "Only bullet.") != 0) { ok = 0; printf("FAIL pptx s2 body=%s\n", b1?b1:"<null>"); }
    }
    wubushow_free(r);
    return ok ? 0 : 1;
}

int main(void) {
    int rc = 0;
    if (check_docx() != 0) rc = 1; else printf("EDIT DOCX OK\n");
    if (check_xlsx() != 0) rc = 1; else printf("EDIT XLSX OK\n");
    if (check_pptx() != 0) rc = 1; else printf("EDIT PPTX OK\n");
    if (rc) { printf("EDIT ROUNDTRIP FAILED\n"); return 1; }
    printf("EDIT ROUNDTRIP PASSED (docx + xlsx + pptx lossless)\n");
    return 0;
}
