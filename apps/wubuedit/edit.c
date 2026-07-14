#include "edit.h"
#include "docmodel.h"
#include "../wubuoxml/reader.h"
#include "../wubuoxml/docx_text.h"
#include "word.h"
#include "assemble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Structure-preserving round-trip: parse word/document.xml into a model, then
 * re-emit it through the WordprocessingML writer. Paragraph style, bold runs
 * and tables survive the reader+writer loop (unlike the old text-only path). */
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

    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!doc) {
        fprintf(stderr, "wubuedit: no word/document.xml\n");
        wubuoxml_free(&pkg); free(data);
        return -1;
    }

    dm_doc model;
    if (wubuedit_docmodel_parse(doc->bytes, doc->len, &model) != 0) {
        wubuoxml_free(&pkg); free(data);
        return -1;
    }
    wubuoxml_free(&pkg);
    free(data);

    wubuword_doc *d = wubuword_create();
    for (size_t i = 0; i < model.n; i++) {
        dm_block *b = &model.blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            wubuword_para(d, b->para.style, b->para.bold, b->para.text ? b->para.text : "");
        } else if (b->kind == DM_BLOCK_TABLE) {
            wubuword_table_begin(d);
            for (size_t r = 0; r < b->table.rows; r++) {
                wubuword_row(d);
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cell = b->table.cells[r * b->table.cols + c];
                    const char *t = cell ? (cell->text ? cell->text : "") : "";
                    wubuword_cell(d, cell ? cell->bold : 0, t);
                }
            }
            wubuword_table_end(d);
        }
    }

    size_t dlen = 0;
    char *docxml = wubuword_render(d, &dlen);
    wubuword_free(d);
    int rc = wubuword_assemble(out_path, docxml, dlen);
    free(docxml);
    wubuedit_docmodel_free(&model);
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
