#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../src/wubuoxml/reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generate a docx, read it back with the OPC reader, and confirm we can
 * extract the paragraph text. Verifies writer+reader round-trip. */
int main(void) {
    const char *path = "/tmp/test_reader_roundtrip.docx";
    wubuword_doc *d = wubuword_create();
    wubuword_para(d, "Title", 1, "Roundtrip Title");
    wubuword_para(d, NULL, 0, "Body text extracted by wuburead.");
    size_t len = 0;
    char *doc = wubuword_render(d, &len);
    wubuword_free(d);
    if (wubuword_assemble(path, doc, len) != 0) { free(doc); printf("FAIL assemble\n"); return 1; }
    free(doc);

    FILE *f = fopen(path, "rb");
    if (!f) { printf("FAIL open\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)sz);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return 1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { printf("FAIL read package\n"); free(data); return 1; }
    const wubuoxml_part *docpart = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!docpart) { printf("FAIL no document.xml\n"); wubuoxml_free(&pkg); free(data); return 1; }
    char *txt = NULL;
    int rc = wubuoxml_docx_text(docpart->bytes, docpart->len, &txt);
    int ok = (rc == 0 && txt && strstr(txt, "Roundtrip Title") && strstr(txt, "Body text extracted"));
    if (!ok) printf("FAIL text extract: '%s'\n", txt ? txt : "(null)");
    free(txt);
    wubuoxml_free(&pkg);
    free(data);
    if (!ok) { printf("READER ROUNDTRIP FAILED\n"); return 1; }
    printf("READER ROUNDTRIP PASSED\n");
    return 0;
}
