/* WS11 model -> OOXML round-trip: build a model, write .docx, read it back,
 * and confirm the text survives. Exercises wubumodel containment + docx writer
 * + the existing wubuoxml reader/extractor (the real integration seam). */
#include "model.h"
#include "reader.h"
#include "docx_text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    wubumodel_doc *doc = wubumodel_doc_create();
    assert(doc);

    wubumodel_node *sect = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(doc, WUBUMODEL_RUN);
    assert(sect && para && run);
    assert(wubumodel_node_append(doc, sect, para) == 0);
    assert(wubumodel_node_append(doc, para, run) == 0);
    assert(wubumodel_run_set_text(run, "Unified model -> docx round-trip works.") == 0);

    const char *out = "/tmp/ws11_roundtrip.docx";
    assert(wubumodel_write_docx(doc, out) == 0);
    wubumodel_doc_destroy(doc);

    /* read the package back */
    FILE *fp = fopen(out, "rb");
    assert(fp);
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *buf = malloc(sz); assert(buf);
    assert(fread(buf, 1, sz, fp) == (size_t)sz);
    fclose(fp);

    wubuoxml_package p = {0};
    assert(wubuoxml_read(buf, sz, &p) == 0);
    const wubuoxml_part *docxml = wubuoxml_part_find(&p, "word/document.xml");
    assert(docxml && "word/document.xml present");

    char *text = NULL;
    assert(wubuoxml_docx_text(docxml->bytes, docxml->len, &text) == 0);
    assert(text && strstr(text, "Unified model -> docx round-trip works.") != NULL);
    printf("model_io: round-trip OK; extracted: \"%s\"\n", text);

    free(text);
    wubuoxml_free(&p);
    free(buf);
    return 0;
}
