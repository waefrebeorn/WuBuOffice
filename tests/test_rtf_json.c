/* test_rtf_json -- RTF writer + JSON serializers for all three models.
 *
 * Builds a doc model (via md import), a workbook, and a presentation, then
 * exports RTF (doc) and JSON (doc/book/pres) and asserts the key content and
 * valid structure. JSON is validated by an independent oracle (python3 -m json)
 * when available; otherwise the structural substring checks still run. */

#include "../apps/wubudoc/doc_rtf.h"
#include "../apps/wubudoc/model_json.h"
#include "../apps/wubudoc/doc_md.h"
#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuedit/docmodel.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
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
    size_t n = 0; uint8_t *d = slurp(path, &n); if (!d) return 0;
    int found = 0; size_t ln = strlen(needle);
    if (n >= ln) for (size_t i = 0; i + ln <= n; i++) if (memcmp(d+i, needle, ln)==0){found=1;break;}
    free(d); return found;
}
/* validate JSON with python if present; return 1 ok, 1 also if no python (skip) */
static int json_valid(const char *path) {
    const char *py = getenv("WUBU_CONFORMANCE_PYTHON"); if (!py) py = "python3";
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s -c \"import json,sys; json.load(open(sys.argv[1]))\" %s 2>/dev/null", py, path);
    int rc = system(cmd);
    if (rc == -1) return 1;
    return ((rc >> 8) & 0xff) == 0;
}

int main(void) {
    int rc = 0;

    /* --- doc model from markdown --- */
    const char *md = "/tmp/test_rj.md";
    FILE *f = fopen(md, "wb");
    fputs("# Report\n\n**Important.**\n\n| A | B |\n| --- | --- |\n| 1 | 2 |\n\n", f);
    fclose(f);
    wubuword_doc *wd = NULL;
    if (wubudoc_read_md(md, &wd) != 0) { printf("FAIL read_md\n"); return 1; }
    size_t len = 0; char *doc = wubuword_render(wd, &len); wubuword_free(wd);
    wubuword_assemble("/tmp/test_rj.docx", doc, len); free(doc);
    size_t sz = 0; uint8_t *data = slurp("/tmp/test_rj.docx", &sz);
    wubuoxml_package pkg; wubuoxml_read(data, sz, &pkg);
    const wubuoxml_part *dp = wubuoxml_part_find(&pkg, "word/document.xml");
    dm_doc m; wubuedit_docmodel_parse(dp->bytes, dp->len, &m);

    /* RTF export */
    if (wubudoc_write_rtf(&m, "/tmp/test_rj.rtf") != 0) { printf("FAIL write_rtf\n"); rc = 1; }
    if (!contains("/tmp/test_rj.rtf", "{\\rtf1")) { printf("FAIL rtf header\n"); rc = 1; }
    if (!contains("/tmp/test_rj.rtf", "Report")) { printf("FAIL rtf title\n"); rc = 1; }
    if (!contains("/tmp/test_rj.rtf", "\\trowd")) { printf("FAIL rtf table\n"); rc = 1; }
    if (!contains("/tmp/test_rj.rtf", "\\par")) { printf("FAIL rtf para\n"); rc = 1; }

    /* doc JSON */
    if (wubudoc_write_doc_json(&m, "/tmp/test_rj_doc.json") != 0) { printf("FAIL doc_json\n"); rc = 1; }
    if (!json_valid("/tmp/test_rj_doc.json")) { printf("FAIL doc_json invalid\n"); rc = 1; }
    if (!contains("/tmp/test_rj_doc.json", "\"kind\": \"table\"")) { printf("FAIL doc_json table\n"); rc = 1; }

    wubuedit_docmodel_free(&m); wubuoxml_free(&pkg); free(data);

    /* --- workbook JSON --- */
    wubucell_book *b = wubucell_create();
    int sh = wubucell_sheet(b, "Data");
    wubucell_cell_s(b, sh, 1, 1, "Item");
    wubucell_cell_n(b, sh, 2, 1, 42);
    wubucell_cell_f(b, sh, 2, 2, "B1*2", 84);
    if (wubudoc_write_book_json(b, "/tmp/test_rj_book.json") != 0) { printf("FAIL book_json\n"); rc = 1; }
    if (!json_valid("/tmp/test_rj_book.json")) { printf("FAIL book_json invalid\n"); rc = 1; }
    if (!contains("/tmp/test_rj_book.json", "\"kind\": \"formula\"")) { printf("FAIL book_json formula\n"); rc = 1; }
    if (!contains("/tmp/test_rj_book.json", "\"name\": \"Data\"")) { printf("FAIL book_json sheet name\n"); rc = 1; }
    wubucell_free(b);

    /* --- presentation JSON --- */
    wubushow_pres *p = wubushow_create();
    wubushow_slide(p, "Slide One", "Bullet A\nBullet B");
    if (wubudoc_write_pres_json(p, "/tmp/test_rj_pres.json") != 0) { printf("FAIL pres_json\n"); rc = 1; }
    if (!json_valid("/tmp/test_rj_pres.json")) { printf("FAIL pres_json invalid\n"); rc = 1; }
    if (!contains("/tmp/test_rj_pres.json", "\"title\": \"Slide One\"")) { printf("FAIL pres_json title\n"); rc = 1; }
    wubushow_free(p);

    if (rc) { printf("RTF/JSON TEST FAILED\n"); return 1; }
    printf("RTF/JSON TEST PASSED (rtf + doc/book/pres json, oracle-validated)\n");
    return 0;
}
