/* test_md -- Markdown import + Markdown/HTML export over the document model.
 *
 * Loop: write a Markdown file -> wubudoc_read_md -> wubuword_render -> assemble
 * .docx -> read the package -> docmodel_parse -> assert structure -> export to
 * Markdown and HTML and assert the key content survives. Proves docx <-> md/html
 * conversion works both directions. */

#include "../apps/wubudoc/doc_md.h"
#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuedit/docmodel.h"
#include "../src/wubuoxml/reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *p, size_t *o) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(s ? (size_t)s : 1);
    if (fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f); *o = (size_t)s; return d;
}

static int contains(const char *path, const char *needle) {
    size_t n = 0; uint8_t *d = slurp(path, &n);
    if (!d) return 0;
    int found = 0;
    if (n >= strlen(needle)) {
        for (size_t i = 0; i + strlen(needle) <= n; i++)
            if (memcmp(d + i, needle, strlen(needle)) == 0) { found = 1; break; }
    }
    free(d);
    return found;
}

int main(void) {
    const char *md = "/tmp/test_md_in.md";
    FILE *f = fopen(md, "wb");
    fputs("# The Title\n\n", f);
    fputs("## Section Two\n\n", f);
    fputs("A normal paragraph.\n\n", f);
    fputs("**Bold statement.**\n\n", f);
    fputs("- first bullet\n", f);
    fputs("- second bullet\n\n", f);
    fputs("| Name | Cost |\n", f);
    fputs("| --- | --- |\n", f);
    fputs("| Engine | 1200 |\n", f);
    fputs("| Docs | 320 |\n\n", f);
    fclose(f);

    /* MD -> doc -> docx */
    wubuword_doc *d = NULL;
    if (wubudoc_read_md(md, &d) != 0) { printf("FAIL read_md\n"); return 1; }
    size_t len = 0; char *doc = wubuword_render(d, &len); wubuword_free(d);
    if (wubuword_assemble("/tmp/test_md.docx", doc, len) != 0) { free(doc); printf("FAIL assemble\n"); return 1; }
    free(doc);

    /* read docx back -> model */
    size_t sz = 0; uint8_t *data = slurp("/tmp/test_md.docx", &sz);
    wubuoxml_package pkg;
    if (wubuoxml_read(data, sz, &pkg) != 0) { free(data); printf("FAIL read docx\n"); return 1; }
    const wubuoxml_part *dp = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!dp) { wubuoxml_free(&pkg); free(data); printf("FAIL no document.xml\n"); return 1; }
    dm_doc m;
    if (wubuedit_docmodel_parse(dp->bytes, dp->len, &m) != 0) { wubuoxml_free(&pkg); free(data); printf("FAIL parse\n"); return 1; }

    int rc = 0;
    int saw_h1 = 0, saw_h2 = 0, saw_bold = 0, saw_table = 0, table_ok = 0;
    for (size_t i = 0; i < m.n; i++) {
        dm_block *b = &m.blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            if (b->para.style && strcmp(b->para.style, "Heading1") == 0) saw_h1 = 1;
            if (b->para.style && strcmp(b->para.style, "Heading2") == 0) saw_h2 = 1;
            if (b->para.bold) saw_bold = 1;
        } else if (b->kind == DM_BLOCK_TABLE) {
            saw_table = 1;
            if (b->table.rows == 3 && b->table.cols == 2) table_ok = 1;
        }
    }
    if (!saw_h1) { printf("FAIL no Heading1\n"); rc = 1; }
    if (!saw_h2) { printf("FAIL no Heading2\n"); rc = 1; }
    if (!saw_bold) { printf("FAIL no bold paragraph\n"); rc = 1; }
    if (!saw_table || !table_ok) { printf("FAIL table (saw=%d ok=%d)\n", saw_table, table_ok); rc = 1; }

    /* export MD + HTML and check content survives */
    if (wubudoc_write_md(&m, "/tmp/test_md_out.md") != 0) { printf("FAIL write_md\n"); rc = 1; }
    if (wubudoc_write_html(&m, "/tmp/test_md_out.html") != 0) { printf("FAIL write_html\n"); rc = 1; }
    if (!contains("/tmp/test_md_out.md", "# The Title")) { printf("FAIL md missing H1\n"); rc = 1; }
    if (!contains("/tmp/test_md_out.md", "| Engine |")) { printf("FAIL md missing table\n"); rc = 1; }
    if (!contains("/tmp/test_md_out.html", "<h1>The Title</h1>")) { printf("FAIL html missing H1\n"); rc = 1; }
    if (!contains("/tmp/test_md_out.html", "<table")) { printf("FAIL html missing table\n"); rc = 1; }

    wubuedit_docmodel_free(&m);
    wubuoxml_free(&pkg);
    free(data);
    if (rc) { printf("MD TEST FAILED\n"); return 1; }
    printf("MD TEST PASSED (md -> docx -> model -> md/html, headings+bold+table)\n");
    return 0;
}
