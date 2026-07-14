/* test_readers -- reverse-direction readers for WuBuOffice convert.
 *
 * Builds a .docx with the wubuword generator, converts it to rtf / html / epub
 * (all of which we WRITE), then reads each back through the new READERs
 * (wuburead_rtf / wuburead_html / wuburead_epub) and checks the recovered
 * dm_doc text matches. This proves the read side of rtf/html/epub is not just
 * linked but correct. No third-party oracle required (pure C round-trip). */

#include "../apps/wubuword/assemble.h"
#include "../apps/wubuword/word.h"
#include "../apps/wubuconv/conv_map.h"
#include "../apps/wuburead/readers.h"
#include "../apps/wubuedit/docmodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* count paragraphs whose text contains `needle` */
static int doc_has_text(const dm_doc *d, const char *needle) {
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA && b->para.text && strstr(b->para.text, needle))
            return 1;
    }
    return 0;
}

static int has_table(const dm_doc *d) {
    for (size_t i = 0; i < d->n; i++)
        if (d->blocks[i].kind == DM_BLOCK_TABLE) return 1;
    return 0;
}

int main(void) {
    /* generate a rich source docx */
    wubuword_doc *w = wubuword_create();
    wubuword_para(w, "Title", 0, "Reader Test");
    wubuword_para(w, "Heading1", 0, "Section One");
    wubuword_para(w, NULL, 1, "Bold line of text.");
    wubuword_table_begin(w);
    wubuword_row(w);
    wubuword_cell(w, 1, "Name"); wubuword_cell(w, 1, "Kind");
    wubuword_row(w);
    wubuword_cell(w, 0, "wubuzip"); wubuword_cell(w, 0, "ZIP");
    size_t len = 0; char *xml = wubuword_render(w, &len);
    wubuword_assemble("/tmp/readtest_in.docx", xml, len);
    free(xml); wubuword_free(w);

    int fail = 0;

    /* emit the three write-only formats we can now read back */
    const char *fmts[] = {"rtf", "html", "epub"};
    for (int i = 0; i < 3; i++) {
        char out[256];
        snprintf(out, sizeof out, "/tmp/readtest_in.%s", fmts[i]);
        if (wubuconv_convert("/tmp/readtest_in.docx", out) != 0) {
            printf("FAIL emit %s\n", fmts[i]); fail = 1; continue;
        }
        dm_doc d; memset(&d, 0, sizeof d);
        int rc = -1;
        if (strcmp(fmts[i], "rtf") == 0)  rc = wuburead_rtf(out, &d);
        else if (strcmp(fmts[i], "html") == 0) rc = wuburead_html(out, &d);
        else rc = wuburead_epub(out, &d);
        if (rc != 0) { printf("FAIL read %s (rc=%d)\n", fmts[i], rc); fail = 1; wubuedit_docmodel_free(&d); continue; }
        /* the reader must recover the title, the heading, the bold line, and
         * the 2x2 table. */
        if (!doc_has_text(&d, "Reader Test")) { printf("FAIL %s missing title\n", fmts[i]); fail = 1; }
        if (!doc_has_text(&d, "Section One")) { printf("FAIL %s missing heading\n", fmts[i]); fail = 1; }
        if (!doc_has_text(&d, "Bold line"))   { printf("FAIL %s missing body\n", fmts[i]); fail = 1; }
        if (!has_table(&d))                    { printf("FAIL %s missing table\n", fmts[i]); fail = 1; }
        wubuedit_docmodel_free(&d);
    }

    if (fail) { printf("READERS TEST FAILED\n"); return 1; }
    printf("READERS TEST PASSED (rtf/html/epub -> dm_doc round-trip, text + table recovered)\n");
    return 0;
}
