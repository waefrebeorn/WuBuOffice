/* WuBuOffice -- apps/wubuword/word_internal
 * Internal definitions for the WordprocessingML (docx) builder. The wubuword_doc
 * struct is opaque to callers (see word.h); only word_*.c include this header.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#ifndef WUBUWORD_WORD_INTERNAL_H
#define WUBUWORD_WORD_INTERNAL_H

#include "word.h"

struct wubuword_doc {
    wubuxml_writer *w;
    FILE *m;          /* memstream we own; closed on render */
    char *buf; size_t len;
    int in_table;
    int opened;       /* body/document opened */
};

/* doc.c */
void word_ensure_root(wubuword_doc *d);

#endif /* WUBUWORD_WORD_INTERNAL_H */
