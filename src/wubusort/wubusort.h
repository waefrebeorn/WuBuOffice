/* wubusort.h — stable multi-column sort over opaque rows. */
#ifndef WUBUSORT_H
#define WUBUSORT_H
#include <stddef.h>

/* Extract the string value of `col` from `row`. Return NULL for empty/absent. */
typedef const char *(*wubusort_cellfn)(void *row, int col, void *ud);

typedef struct {
    int col;      /* column index */
    int desc;     /* 1 = descending */
    int numeric;  /* 1 = compare as numbers (decimal), 0 = lexicographic */
} wubusort_col;

/* Stable sort `rows[0..n)` in place by the ordered list of columns. A column
 * comparison only breaks ties (next column decides) when equal under the
 * current column's mode. Returns 0 on success, -1 on bad input. */
int wubusort_rows(void **rows, size_t n,
                  const wubusort_col *cols, int ncols,
                  wubusort_cellfn cell, void *ud);

/* Numeric comparison helper: returns <0,0,>0 comparing two decimal strings
 * (or NULL/empty = 0). Robust to leading +/-, leading zeros, fractions. */
int wubusort_numcmp(const char *a, const char *b);

#endif
