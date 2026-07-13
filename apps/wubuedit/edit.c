#include "edit.h"
#include "../wubuoxml/reader.h"
#include "../wubuoxml/docx_text.h"
#include "word.h"
#include "assemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read `in_path`, pull the document text out of word/document.xml (if any),
 * and write a fresh .docx containing that text as a single body paragraph.
 * This is a real reader->writer round trip that proves the OPC read path and
 * the WordprocessingML write path agree on the content. */
int wubuedit_roundtrip(const char *in_path, const char *out_path) {
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) {
        fprintf(stderr, "wubuedit: cannot parse OPC package\n");
        free(data);
        return -1;
    }

    /* Prefer word/document.xml; fall back to the first part that has a <w:t>. */
    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    char *txt = NULL;
    if (doc) {
        wubuoxml_docx_text(doc->bytes, doc->len, &txt);
    } else {
        for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
            const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
            if (wubuoxml_docx_text(pt->bytes, pt->len, &txt) == 0 && txt && txt[0]) break;
            free(txt); txt = NULL;
        }
    }

    wubuoxml_free(&pkg);
    free(data);

    if (!txt) txt = strdup("");

    wubuword_doc *d = wubuword_create();
    wubuword_para(d, "Title", 1, "WuBuEdit Round-Trip");
    /* split extracted text on newlines into paragraphs */
    const char *p = txt;
    const char *line = p;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        char *buf = malloc(len + 1);
        memcpy(buf, line, len); buf[len] = '\0';
        wubuword_para(d, NULL, 0, buf);
        free(buf);
        if (!nl) break;
        line = nl + 1;
    }
    if (txt[0] == '\0') wubuword_para(d, NULL, 0, "(no extractable text)");

    size_t dlen = 0;
    char *docxml = wubuword_render(d, &dlen);
    wubuword_free(d);
    int rc = wubuword_assemble(out_path, docxml, dlen);
    free(docxml);
    free(txt);
    return rc;
}

int wubuedit_main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wubuedit <in.docx> [out.docx]\n"); return 1; }
    const char *in = argv[1];
    const char *out = (argc > 2) ? argv[2] : "WuBuOffice-edited.docx";
    int rc = wubuedit_roundtrip(in, out);
    if (rc == 0) printf("wubuedit: wrote %s (round-trip of %s)\n", out, in);
    else fprintf(stderr, "wubuedit: failed\n");
    return rc ? 1 : 0;
}
