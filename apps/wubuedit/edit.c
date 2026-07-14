#include "edit.h"
#include "docmodel.h"
#include "../wubuoxml/reader.h"
#include "../wubuoxml/docx_text.h"
#include "word.h"
#include "assemble.h"
#include "../wubucell/cell.h"
#include "../wubucell/cell_read.h"
#include "../wubushow/show.h"
#include "../wubushow/show_read.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Structure-preserving round-trip for each supported format: parse the
 * document part into our model, then re-emit it through the matching writer.
 * Paragraph style, bold runs, tables (docx), cells + formulas (xlsx) and
 * slide title/body (pptx) survive the reader+writer loop (unlike the old
 * text-only path). */

static int roundtrip_docx(const uint8_t *data, size_t sz, const char *out_path) {
    wubuoxml_package pkg;
    if (wubuoxml_read(data, sz, &pkg) != 0) {
        fprintf(stderr, "wubuedit: cannot parse OPC package\n");
        return -1;
    }
    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!doc) {
        fprintf(stderr, "wubuedit: no word/document.xml\n");
        wubuoxml_free(&pkg);
        return -1;
    }
    dm_doc model;
    if (wubuedit_docmodel_parse(doc->bytes, doc->len, &model) != 0) {
        wubuoxml_free(&pkg);
        return -1;
    }
    wubuoxml_free(&pkg);

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
    wubuoxml_free(&pkg);
    size_t dlen = 0;
    char *docxml = wubuword_render(d, &dlen);
    wubuword_free(d);
    int rc = wubuword_assemble(out_path, docxml, dlen);
    free(docxml);
    wubuedit_docmodel_free(&model);
    return rc;
}

int wubuedit_roundtrip(const char *in_path, const char *out_path) {
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    /* dispatch by extension */
    size_t ln = strlen(in_path);
    int is_docx = ln >= 5 && strcmp(in_path + ln - 5, ".docx") == 0;
    int is_xlsx = ln >= 5 && strcmp(in_path + ln - 5, ".xlsx") == 0;
    int is_pptx = ln >= 5 && strcmp(in_path + ln - 5, ".pptx") == 0;

    int rc;
    if (is_docx) {
        rc = roundtrip_docx(data, (size_t)sz, out_path);
    } else if (is_xlsx) {
        wubucell_book *b = NULL;
        if (wubucell_read(in_path, &b) != 0) { fprintf(stderr, "wubuedit: cannot read xlsx\n"); free(data); return -1; }
        rc = wubucell_assemble(b, out_path);
        wubucell_free(b);
    } else if (is_pptx) {
        wubushow_pres *p = NULL;
        if (wubushow_read(in_path, &p) != 0) { fprintf(stderr, "wubuedit: cannot read pptx\n"); free(data); return -1; }
        rc = wubushow_assemble(p, out_path);
        wubushow_free(p);
    } else {
        fprintf(stderr, "wubuedit: unsupported input type (need .docx/.xlsx/.pptx): %s\n", in_path);
        free(data);
        return -1;
    }
    free(data);
    return rc;
}

int wubuedit_main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wubuedit <in.docx|in.xlsx|in.pptx> [out.<ext>]\n"); return 1; }
    const char *in = argv[1];
    const char *out = (argc > 2) ? argv[2] : NULL;
    if (!out) {
        /* default output: same stem + .edited.<ext> */
        size_t il = strlen(in);
        size_t el = 5; /* .docx/.xlsx/.pptx are all 5 chars */
        char *def = malloc(il + 8);
        memcpy(def, in, il - el);
        strcpy(def + il - el, ".edited");
        strcpy(def + il - el + 7, in + il - el);
        out = def;
    }
    int rc = wubuedit_roundtrip(in, out);
    if (rc == 0) printf("wubuedit: wrote %s (round-trip of %s)\n", out, in);
    else fprintf(stderr, "wubuedit: failed\n");
    return rc ? 1 : 0;
}
