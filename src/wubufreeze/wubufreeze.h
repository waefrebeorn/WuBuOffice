/* wubufreeze.h — spreadsheet freeze panes: frozen header rows/cols + visible
 * scroll origin. Pure view-state model (no rendering). */
#ifndef WUBUFREEZE_H
#define WUBUFREEZE_H

typedef struct {
    int frozen_rows;     /* rows locked at top */
    int frozen_cols;     /* cols locked at left */
    int active_row;      /* current scroll anchor row */
    int active_col;
} wubufreeze;

/* Initialize with frozen row/col counts. Returns 0. */
int wubufreeze_init(wubufreeze *f, int fr, int fc);

/* Scroll so `row` is the top visible scrollable row. Returns 0. */
int wubufreeze_scroll(wubufreeze *f, int row, int col);

/* The visible row index a logical row maps to (accounting for frozen rows):
 * returns -1 if `row` is in the frozen region, else the offset into the
 * scrollable region. */
int wubufreeze_visible_row(const wubufreeze *f, int row);
int wubufreeze_visible_col(const wubufreeze *f, int col);

int wubufreeze_frozen_rows(const wubufreeze *f);
int wubufreeze_frozen_cols(const wubufreeze *f);

#endif
