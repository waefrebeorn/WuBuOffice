/* wubufilter.h — spreadsheet AutoFilter: predicate row filter. */
#ifndef WUBUFILTER_H
#define WUBUFILTER_H
#include <stddef.h>

typedef enum {
    WUBUFILTER_EQ,       /* cell == value (string) */
    WUBUFILTER_NEQ,
    WUBUFILTER_CONTAINS, /* cell contains value (case-insensitive) */
    WUBUFILTER_GT,       /* numeric > value */
    WUBUFILTER_GTE,
    WUBUFILTER_LT,
    WUBUFILTER_LTE,
    WUBUFILTER_BETWEEN,  /* numeric value in [lo,hi]; arg must be "lo|hi" */
    WUBUFILTER_TOP,      /* numeric top N by value (arg = count as string) */
    WUBUFILTER_BLANK,    /* cell empty/NULL */
    WUBUFILTER_NOTBLANK
} wubufilter_op;

typedef struct {
    int col;             /* column index */
    wubufilter_op op;
    const char *value;   /* argument (for BETWEEN: "lo|hi"; TOP: "N") */
} wubufilter_crit;

typedef const char *(*wubufilter_cellfn)(void *row, int col, void *ud);

/* Fill `out[0..*n)` with the indices of rows (from `rows[0..nrows)`) that
 * satisfy EVERY criterion in `crit[0..ncrit)`. Rows in the result keep their
 * original order. Caller pre-sizes `out` to at least `nrows`. Returns 0 on
 * success, -1 on bad args. */
int wubufilter_apply(void **rows, size_t nrows,
                     const wubufilter_crit *crit, int ncrit,
                     wubufilter_cellfn cell, void *ud,
                     size_t *out, size_t *n);

/* Numeric parse helper (NULL/empty -> 0). */
double wubufilter_num(const char *s);

#endif
