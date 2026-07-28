/* pdfextract.c -- PDF text import (EXP-91). See pdfextract.h. */
#include "pdfextract.h"
#include "pdf_extract.h"   /* wubupdf: clean-room PDF text extraction */

#include <stdlib.h>
#include <stdio.h>

char *pdfextract_bytes(const uint8_t *data, size_t len){
    if (!data || len==0) return NULL;
    return pdf_extract_text(data, len);
}

char *pdfextract_file(const char *path){
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0){ fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf){ fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, f); fclose(f);
    char *txt = pdf_extract_text(buf, r);
    free(buf);
    return txt;
}
