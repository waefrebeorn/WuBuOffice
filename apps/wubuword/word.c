#include "word.h"
#include <stdlib.h>
#include <string.h>

struct wubuword_doc {
    wubuxml_writer *w;
    FILE *m;          /* memstream we own; closed on render */
    char *buf; size_t len;
    int in_table;
    int opened;       /* body/document opened */
};

wubuword_doc *wubuword_create(void) {
    wubuword_doc *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    return d;
}
void wubuword_free(wubuword_doc *d) {
    if (!d) return;
    if (d->w) wubuxml_destroy(d->w);  /* closes owned memstream if not yet rendered */
    if (d->m) { fflush(d->m); fclose(d->m); } /* if render not called */
    free(d->buf);
    free(d);
}

static void ensure_root(wubuword_doc *d) {
    if (d->opened) return;
    if (!d->m) d->m = open_memstream(&d->buf, &d->len);
    d->w = wubuxml_create(d->m);
    wubuxml_open(d->w, "w:document");
    wubuxml_set_attr(d->w, "xmlns:w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
    wubuxml_open(d->w, "w:body");
    d->opened = 1;
}

void wubuword_para(wubuword_doc *d, const char *style, int bold, const char *text) {
    ensure_root(d);
    wubuxml_open(d->w, "w:p");
    if (style) {
        wubuxml_open(d->w, "w:pPr");
        wubuxml_open(d->w, "w:pStyle");
        wubuxml_set_attr(d->w, "w:val", style);
        wubuxml_close(d->w); wubuxml_close(d->w);
    }
    wubuxml_open(d->w, "w:r");
    if (bold) {
        wubuxml_open(d->w, "w:rPr");
        wubuxml_open(d->w, "w:b"); wubuxml_close(d->w);
        wubuxml_close(d->w);
    }
    wubuxml_open(d->w, "w:t");
    wubuxml_set_attr(d->w, "xml:space", "preserve");
    wubuxml_text(d->w, text);
    wubuxml_close(d->w); wubuxml_close(d->w); wubuxml_close(d->w);
}

static void cell_para(wubuword_doc *d, int bold, const char *text) {
    wubuxml_open(d->w, "w:p");
    wubuxml_open(d->w, "w:r");
    if (bold) {
        wubuxml_open(d->w, "w:rPr");
        wubuxml_open(d->w, "w:b"); wubuxml_close(d->w);
        wubuxml_close(d->w);
    }
    wubuxml_open(d->w, "w:t");
    wubuxml_set_attr(d->w, "xml:space", "preserve");
    wubuxml_text(d->w, text);
    wubuxml_close(d->w); wubuxml_close(d->w); wubuxml_close(d->w);
}

void wubuword_table_begin(wubuword_doc *d) {
    ensure_root(d);
    wubuxml_open(d->w, "w:tbl");
    wubuxml_open(d->w, "w:tblPr");
    wubuxml_open(d->w, "w:tblBorders");
    static const char *edges[] = {"w:top","w:left","w:bottom","w:right","w:insideH","w:insideV",NULL};
    for (int i = 0; edges[i]; i++) {
        wubuxml_open(d->w, edges[i]);
        wubuxml_set_attr(d->w, "w:val", "single");
        wubuxml_set_attr(d->w, "w:sz", "4");
        wubuxml_set_attr(d->w, "w:space", "0");
        wubuxml_set_attr(d->w, "w:color", "auto");
        wubuxml_close(d->w);
    }
    wubuxml_close(d->w); wubuxml_close(d->w);
    d->in_table = 1;
}

void wubuword_cell(wubuword_doc *d, int bold, const char *text) {
    if (!d->in_table) return;
    wubuxml_open(d->w, "w:tr");
    wubuxml_open(d->w, "w:tc");
    wubuxml_open(d->w, "w:tcPr");
    wubuxml_open(d->w, "w:tcW");
    wubuxml_set_attr(d->w, "w:w", "2400");
    wubuxml_set_attr(d->w, "w:type", "dxa");
    wubuxml_close(d->w); wubuxml_close(d->w);
    cell_para(d, bold, text);
    wubuxml_close(d->w); wubuxml_close(d->w);
}

void wubuword_table_end(wubuword_doc *d) {
    if (!d->in_table) return;
    wubuxml_close(d->w); /* tbl */
    d->in_table = 0;
}

char *wubuword_render(wubuword_doc *d, size_t *out_len) {
    ensure_root(d);
    wubuxml_open(d->w, "w:sectPr");
    wubuxml_open(d->w, "w:pgSz");
    wubuxml_set_attr(d->w, "w:w", "12240");
    wubuxml_set_attr(d->w, "w:h", "15840");
    wubuxml_close(d->w); wubuxml_close(d->w);
    wubuxml_close(d->w); /* body */
    wubuxml_close(d->w); /* document */
    wubuxml_destroy(d->w);
    d->w = NULL;
    fflush(d->m); fclose(d->m); d->m = NULL;
    if (out_len) *out_len = d->len;
    char *result = d->buf;   /* ownership transferred to caller */
    d->buf = NULL;
    return result;
}
