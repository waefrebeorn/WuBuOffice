#ifndef WUBUEDIT_DOCMODEL_H
#define WUBUEDIT_DOCMODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A parsed WordprocessingML document model, reconstructed from
 * word/document.xml so wubuedit can re-emit it (round-trip) without losing
 * the structure the writer supports: paragraph style, bold runs, and tables.
 *
 * Opaque to callers; built by wubuedit_docmodel_parse() and freed by
 * wubuedit_docmodel_free(). */

typedef enum { DM_BLOCK_PARA, DM_BLOCK_TABLE } dm_block_kind;

typedef struct {
    char *style;   /* pStyle @w:val, e.g. "Heading1"/"Title", or NULL */
    int   bold;    /* any run in the paragraph carried <w:b> */
    char *text;    /* concatenated run text (entity-decoded) */
} dm_para;

typedef struct {
    dm_para **cells;   /* row-major: cells[row*cols + col] */
    size_t   rows;
    size_t   cols;     /* max cells in any row (rectangularized) */
} dm_table;

typedef struct dm_block {
    dm_block_kind kind;
    dm_para  para;     /* valid if kind == DM_BLOCK_PARA */
    dm_table table;    /* valid if kind == DM_BLOCK_TABLE */
} dm_block;

typedef struct {
    dm_block *blocks;
    size_t n, cap;
} dm_doc;

/* Parse WordprocessingML `xml` (word/document.xml payload) into a model.
 * Returns 0 on success (may be an empty doc) or -1 on alloc failure. */
int wubuedit_docmodel_parse(const uint8_t *xml, size_t len, dm_doc *out);

/* Free a model built by wubuedit_docmodel_parse (safe to call on zeroed/partial). */
void wubuedit_docmodel_free(dm_doc *d);

#ifdef __cplusplus
}
#endif

#endif /* WUBUEDIT_DOCMODEL_H */
