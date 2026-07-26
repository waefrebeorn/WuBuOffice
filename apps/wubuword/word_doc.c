/* WuBuOffice -- apps/wubuword/word_doc
 * WordprocessingML document model + paragraph API + serialization. The flat
 * document model, the paragraph builder, and render live here; tables are in
 * word_table.c. Public API in word.h; internals in word_internal.h.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "word_internal.h"
#include <stdlib.h>
#include <string.h>

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

void word_ensure_root(wubuword_doc *d) {
    if (d->opened) return;
    if (!d->m) d->m = open_memstream(&d->buf, &d->len);
    d->w = wubuxml_create(d->m);
    wubuxml_open(d->w, "w:document");
    wubuxml_set_attr(d->w, "xmlns:w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
    wubuxml_open(d->w, "w:body");
    d->opened = 1;
}

void wubuword_para(wubuword_doc *d, const char *style, int bold, const char *text) {
    word_ensure_root(d);
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

/* Emit a numbered paragraph: a <w:p> carrying <w:pPr><w:numPr> referencing
 * numId (1 = bullet, 2 = decimal). Shared by the list API. */
static void word_numbered_para(wubuword_doc *d, int num_id, const char *text) {
    word_ensure_root(d);
    wubuxml_open(d->w, "w:p");
    wubuxml_open(d->w, "w:pPr");
    wubuxml_open(d->w, "w:numPr");
    wubuxml_open(d->w, "w:ilvl"); wubuxml_set_attr(d->w, "w:val", "0"); wubuxml_close(d->w);
    wubuxml_open(d->w, "w:numId"); wubuxml_set_attr(d->w, "w:val", num_id ? "2" : "1"); wubuxml_close(d->w);
    wubuxml_close(d->w); /* numPr */
    wubuxml_close(d->w); /* pPr */
    wubuxml_open(d->w, "w:r");
    wubuxml_open(d->w, "w:t");
    wubuxml_set_attr(d->w, "xml:space", "preserve");
    wubuxml_text(d->w, text);
    wubuxml_close(d->w); wubuxml_close(d->w); wubuxml_close(d->w);
}

int wubuword_used_lists(const wubuword_doc *d) { return d ? d->used_lists : 0; }

void wubuword_list_begin(wubuword_doc *d, const char *kind) {
    if (!d) return;
    /* numId 1 = bullet, 2 = decimal number */
    d->list_numid = (kind && strcmp(kind, "number") == 0) ? 2 : 1;
    d->in_list = 1;
}

void wubuword_list_item(wubuword_doc *d, const char *text) {
    if (!d) return;
    if (!d->in_list) d->list_numid = 1;   /* default bullet if no begin */
    d->used_lists = 1;
    word_numbered_para(d, d->list_numid == 2, text);
}

void wubuword_list_end(wubuword_doc *d) {
    if (!d) return;
    d->in_list = 0;
}

char *wubuword_render(wubuword_doc *d, size_t *out_len) {
    word_ensure_root(d);
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
