/* WuBuOffice -- apps/wubuword/word_table
 * Table API for the WordprocessingML builder: begin/end a table and append
 * cells. The table builder calls word_ensure_root (doc.c) so a table can be
 * opened before any paragraph. Public API in word.h.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "word_internal.h"

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
    word_ensure_root(d);
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
