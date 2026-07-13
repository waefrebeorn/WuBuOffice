#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuedit/edit.h"
#include "../src/wubuoxml/reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generate a docx, round-trip it through wubuedit, then read the result back
 * and confirm the original title text survived the reader+writer loop. */
int main(void) {
    const char *src = "/tmp/test_edit_src.docx";
    const char *dst = "/tmp/test_edit_dst.docx";

    wubuword_doc *d = wubuword_create();
    wubuword_para(d, "Title", 1, "Roundtrip Title");
    wubuword_para(d, NULL, 0, "Body text preserved by wubuedit round-trip.");
    size_t len = 0;
    char *doc = wubuword_render(d, &len);
    wubuword_free(d);
    if (wubuword_assemble(src, doc, len) != 0) { free(doc); printf("FAIL src assemble\n"); return 1; }
    free(doc);

    if (wubuedit_roundtrip(src, dst) != 0) { printf("FAIL roundtrip\n"); return 1; }

    FILE *f = fopen(dst, "rb");
    if (!f) { printf("FAIL open dst\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)sz);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return 1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { printf("FAIL read dst\n"); free(data); return 1; }
    const wubuoxml_part *docpart = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!docpart) { printf("FAIL no document.xml\n"); wubuoxml_free(&pkg); free(data); return 1; }
    char *txt = NULL;
    wubuoxml_docx_text(docpart->bytes, docpart->len, &txt);
    int ok = (txt && strstr(txt, "Roundtrip Title") && strstr(txt, "Body text preserved"));
    if (!ok) printf("FAIL text: '%s'\n", txt ? txt : "(null)");
    free(txt);
    wubuoxml_free(&pkg);
    free(data);
    if (!ok) { printf("EDIT ROUNDTRIP FAILED\n"); return 1; }
    printf("EDIT ROUNDTRIP PASSED\n");
    return 0;
}
