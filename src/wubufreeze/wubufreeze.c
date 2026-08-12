#include "wubufreeze.h"

int wubufreeze_init(wubufreeze *f, int fr, int fc) {
    if (!f || fr < 0 || fc < 0) return -1;
    f->frozen_rows = fr;
    f->frozen_cols = fc;
    f->active_row = 0;
    f->active_col = 0;
    return 0;
}

int wubufreeze_scroll(wubufreeze *f, int row, int col) {
    if (!f || row < 0 || col < 0) return -1;
    f->active_row = row;
    f->active_col = col;
    return 0;
}

int wubufreeze_visible_row(const wubufreeze *f, int row) {
    if (!f) return -1;
    if (row < f->frozen_rows) return -1; /* frozen region */
    return row - f->frozen_rows - f->active_row;
}

int wubufreeze_visible_col(const wubufreeze *f, int col) {
    if (!f) return -1;
    if (col < f->frozen_cols) return -1;
    return col - f->frozen_cols - f->active_col;
}

int wubufreeze_frozen_rows(const wubufreeze *f) { return f ? f->frozen_rows : 0; }
int wubufreeze_frozen_cols(const wubufreeze *f) { return f ? f->frozen_cols : 0; }
