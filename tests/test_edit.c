#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuedit/edit.h"
#include "../apps/wubuedit/docmodel.h"
#include "../src/wubuoxml/reader.h"
#include "../src/wubuoxml/docx_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Structure-preserving round-trip: build a doc with a Heading1, a bold
 * paragraph and a 2x2 table; round-trip through wubuedit; then re-parse the
 * OUTPUT with the doc model and confirm heading style, bold flag and the
 * table (2 rows x 2 cols) all survived. Sanitizer-clean. */

static uint8_t *slurp(const char *p, size_t *o) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(s ? s : 1);
    if (fread(d, 1, s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f); *o = s; return d;
}

int main(void) {
    const char *src = "/tmp/test_edit_src.docx";
    const char *dst = "/tmp/test_edit_dst.docx";

    wubuword_doc *d = wubuword_create();
    wubuword_para(d, "Heading1", 0, "The Title");
    wubuword_para(d, NULL, 1, "Bold body line.");
    wubuword_table_begin(d);
    wubuword_row(d); wubuword_cell(d, 1, "A1"); wubuword_cell(d, 0, "B1");
    wubuword_row(d); wubuword_cell(d, 0, "A2"); wubuword_cell(d, 1, "B2");
    wubuword_table_end(d);
    size_t len = 0; char *doc = wubuword_render(d, &len); wubuword_free(d);
    if (wubuword_assemble(src, doc, len) != 0) { free(doc); printf("FAIL src\n"); return 1; }
    free(doc);

    if (wubuedit_roundtrip(src, dst) != 0) { printf("FAIL roundtrip\n"); return 1; }

    size_t sz = 0; uint8_t *data = slurp(dst, &sz);
    wubuoxml_package pkg;
    if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); printf("FAIL read dst\n"); return 1; }
    const wubuoxml_part *docpart = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!docpart) { wubuoxml_free(&pkg); free(data); printf("FAIL no document.xml\n"); return 1; }

    dm_doc m;
    if (wubuedit_docmodel_parse(docpart->bytes, docpart->len, &m) != 0) {
        wubuoxml_free(&pkg); free(data); printf("FAIL reparse\n"); return 1;
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
    int ok = saw_heading && saw_bold && saw_table && table_ok;
    if (!ok) printf("FAIL structure: heading=%d bold=%d table=%d(2x2=%d) n=%zu\n",
                    saw_heading, saw_bold, saw_table, table_ok, m.n);
    wubuedit_docmodel_free(&m);
    wubuoxml_free(&pkg);
    free(data);
    if (!ok) { printf("EDIT ROUNDTRIP FAILED\n"); return 1; }
    printf("EDIT ROUNDTRIP PASSED (heading+bold+2x2 table preserved)\n");
    return 0;
}
