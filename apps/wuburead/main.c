#include "../wubuoxml/reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* wuburead: open any OOXML file and dump its parts + extracted text. */
int wuburead_main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: wuburead FILE.docx|xlsx|pptx\n"); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); return 1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { fprintf(stderr, "wuburead: not a valid OPC package\n"); free(data); return 1; }

    printf("PARTS (%zu):\n", wubuoxml_part_count(&pkg));
    for (size_t i = 0; i < wubuoxml_part_count(&pkg); i++) {
        const wubuoxml_part *pt = wubuoxml_part_at(&pkg, i);
        printf("  %s  (%zu bytes)\n", pt->name, pt->len);
        for (size_t k = 0; k < pt->nrel; k++)
            printf("      -> rel %s\n", pt->rel_targets[k]);
    }

    /* try to extract docx text */
    const wubuoxml_part *doc = wubuoxml_part_find(&pkg, "word/document.xml");
    if (doc) {
        char *txt = NULL;
        if (wubuoxml_docx_text(doc->bytes, doc->len, &txt) == 0 && txt) {
            printf("\nDOCX TEXT:\n%s\n", txt);
            free(txt);
        }
    }
    wubuoxml_free(&pkg);
    free(data);
    return 0;
}
