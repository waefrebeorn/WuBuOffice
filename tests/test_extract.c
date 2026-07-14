#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
#include "../src/wubuoxml/reader.h"
#include "../src/wubuoxml/docx_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* End-to-end extraction test: build a docx, an xlsx (shared strings), and a
 * pptx, read each back through the OPC reader, and confirm the SAX-based
 * extractors recover the text. Runs under ASan/UBSan in CI. */

static uint8_t *slurp(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)sz ? (size_t)sz : 1);
    if (fread(d, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(d); return NULL; }
    fclose(f);
    *out_len = (size_t)sz;
    return d;
}

static int check(const char *label, int cond) {
    if (!cond) { printf("FAIL %s\n", label); return 1; }
    printf("ok %s\n", label);
    return 0;
}

int main(void) {
    int fails = 0;

    /* ---------- docx ---------- */
    {
        const char *p = "/tmp/test_extract.docx";
        wubuword_doc *d = wubuword_create();
        wubuword_para(d, "Heading", 1, "Alpha & <Beta>");
        wubuword_para(d, NULL, 0, "Second paragraph.");
        size_t len = 0; char *doc = wubuword_render(d, &len); wubuword_free(d);
        if (wubuword_assemble(p, doc, len) != 0) { free(doc); return 1; }
        free(doc);
        size_t sz; uint8_t *data = slurp(p, &sz);
        wubuoxml_package pkg;
        if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); return 1; }
        const wubuoxml_part *pt = wubuoxml_part_find(&pkg, "word/document.xml");
        char *txt = NULL;
        int rc = wubuoxml_docx_text(pt->bytes, pt->len, &txt);
        fails += check("docx extract", rc == 0 && txt && strstr(txt, "Alpha & <Beta>") && strstr(txt, "Second paragraph."));
        free(txt); wubuoxml_free(&pkg); free(data);
    }

    /* ---------- xlsx (shared strings) ---------- */
    {
        const char *p = "/tmp/test_extract.xlsx";
        wubucell_book *b = wubucell_create();
        wubucell_use_shared_strings(b, 1);
        int s = wubucell_sheet(b, "Sheet1");
        wubucell_cell_s(b, s, 1, 1, "Name");
        wubucell_cell_s(b, s, 2, 1, "Score");
        wubucell_cell_s(b, s, 1, 2, "WuBu");
        wubucell_cell_n(b, s, 2, 2, 42.0);
        wubucell_cell_f(b, s, 2, 3, "B2*2", 84.0);
        if (wubucell_assemble(b, p) != 0) { wubucell_free(b); return 1; }
        wubucell_free(b);

        size_t sz; uint8_t *data = slurp(p, &sz);
        wubuoxml_package pkg;
        if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); return 1; }
        const wubuoxml_part *ss = wubuoxml_part_find(&pkg, "xl/sharedStrings.xml");
        size_t ns = 0;
        const wubuoxml_part *sheets[8];
        for (size_t i = 0; i < wubuoxml_part_count(&pkg) && ns < 8; i++) {
            const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
            if (strncmp(pt->name, "xl/worksheets/sheet", 18) == 0) sheets[ns++] = pt;
        }
        wubuoxml_sheet sh[8];
        for (size_t i = 0; i < ns; i++) { sh[i].name = sheets[i]->name; sh[i].bytes = sheets[i]->bytes; sh[i].len = sheets[i]->len; }
        char *txt = NULL;
        int rc = wubuoxml_xlsx_text(ss ? ss->bytes : NULL, ss ? ss->len : 0, sh, ns, &txt);
        fails += check("xlsx extract (shared strings + formula cached)",
                       rc == 0 && txt && strstr(txt, "Name") && strstr(txt, "Score") && strstr(txt, "WuBu") && strstr(txt, "42") && strstr(txt, "84"));
        free(txt); wubuoxml_free(&pkg); free(data);
    }

    /* ---------- pptx ---------- */
    {
        const char *p = "/tmp/test_extract.pptx";
        wubushow_pres *pr = wubushow_create();
        wubushow_slide(pr, "Title Slide", "First bullet.\nSecond bullet.");
        wubushow_slide(pr, "Next", "More text.");
        if (wubushow_assemble(pr, p) != 0) { wubushow_free(pr); return 1; }
        wubushow_free(pr);

        size_t sz; uint8_t *data = slurp(p, &sz);
        wubuoxml_package pkg;
        if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); return 1; }
        int got = 0;
        for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
            const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
            if (strncmp(pt->name, "ppt/slides/slide", 16) == 0) {
                char *txt = NULL;
                if (wubuoxml_pptx_text(pt->bytes, pt->len, &txt) == 0 && txt) {
                                if (strstr(txt, "Title Slide") || strstr(txt, "Next") || strstr(txt, "First bullet")) got = 1;
                    free(txt);
                }
            }
        }
        fails += check("pptx extract", got);
        wubuoxml_free(&pkg); free(data);
    }

    if (fails) { printf("EXTRACT TEST FAILED (%d)\n", fails); return 1; }
    printf("EXTRACT TEST PASSED\n");
    return 0;
}
