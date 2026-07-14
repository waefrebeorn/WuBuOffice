#include "../wubuoxml/reader.h"
#include "../wubuoxml/docx_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* wuburead: open any OOXML file and dump its parts + extracted text.
 * Dispatches by which parts are present: a word/document.xml part is a .docx,
 * xl/worksheets/... is a .xlsx, ppt/slides/... is a .pptx. */

static void dump_text(const char *label, char *txt) {
    printf("\n%s:\n%s\n", label, txt ? txt : "");
}

int wuburead_main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wuburead FILE.docx|xlsx|pptx\n"); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) {
        fprintf(stderr, "wuburead: not a valid OPC package\n");
        free(data);
        return 1;
    }

    printf("PARTS (%zu):\n", wubuoxml_part_count(&pkg));
    for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
        const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
        printf("  %s  (%zu bytes)\n", pt->name, pt->len);
        for (size_t k = 0; k < pt->nrel; k++)
            printf("      -> rel %s\n", pt->rel_targets[k]);
    }

    /* --- .docx --- */
    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    if (doc) {
        char *txt = NULL;
        if (wubuoxml_docx_text(doc->bytes, doc->len, &txt) == 0) {
            dump_text("DOCX TEXT", txt);
            free(txt);
        }
    }

    /* --- .xlsx --- */
    const wubuoxml_part *ss = wubuoxml_part_find(&pkg, "xl/sharedStrings.xml");
    size_t nsheets = 0;
    for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
        const char *nm = wubuoxml_part_at(&pkg, i)->name;
        if (strncmp(nm, "xl/worksheets/sheet", 18) == 0) nsheets++;
    }
    if (nsheets) {
        wubuoxml_sheet *sh = malloc(nsheets * sizeof *sh);
        size_t j = 0;
        for (size_t i = 0; i < wubuoxml_part_count(&pkg) && j < nsheets; i++) {
            const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
            if (strncmp(pt->name, "xl/worksheets/sheet", 18) == 0) {
                sh[j].name = pt->name; sh[j].bytes = pt->bytes; sh[j].len = pt->len;
                j++;
            }
        }
        char *txt = NULL;
        if (wubuoxml_xlsx_text(ss ? ss->bytes : NULL, ss ? ss->len : 0, sh, nsheets, &txt) == 0) {
            dump_text("XLSX TEXT", txt);
            free(txt);
        }
        free(sh);
    }

    /* --- .pptx --- */
    for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
        const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
        if (strncmp(pt->name, "ppt/slides/slide", 16) == 0) {
            char *txt = NULL;
            if (wubuoxml_pptx_text(pt->bytes, pt->len, &txt) == 0) {
                char label[128];
                snprintf(label, sizeof label, "PPTX TEXT (%s)", pt->name);
                dump_text(label, txt);
                free(txt);
            }
        }
    }

    wubuoxml_free(&pkg);
    free(data);
    return 0;
}
