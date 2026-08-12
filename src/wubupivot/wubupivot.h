/* wubupivot.h — pivot table: cross-tabulate rows by row-dim and col-dim with
 * an aggregate over a measure. */
#ifndef WUBUPIVOT_H
#define WUBUPIVOT_H
#include <stddef.h>

typedef enum {
    WUBUPIV_SUM, WUBUPIV_COUNT, WUBUPIV_AVG, WUBUPIV_MIN, WUBUPIV_MAX
} wubupiv_fn;

typedef struct {
    char **row_keys;   /* distinct row-dim keys (first-seen order) */
    char **col_keys;   /* distinct col-dim keys */
    double *cells;     /* row_keys x col_keys aggregated values */
    size_t nrows, ncols;
    wubupiv_fn fn;
    size_t total_rows; /* input row count (for COUNT semantics) */
} wubupiv;

typedef const char *(*wubupiv_rowfn)(void *row, void *ud);
typedef const char *(*wubupiv_colfn)(void *row, void *ud);
typedef const char *(*wubupiv_valfn)(void *row, void *ud);

/* Build a pivot from `rows[0..n)` grouped by row/col keys. Returns a new
 * wubupiv* (caller frees with wubupiv_free) or NULL on OOM/bad args. */
wubupiv *wubupiv_build(void **rows, size_t n,
                       wubupiv_rowfn rfn, wubupiv_colfn cfn, wubupiv_valfn vfn,
                       wubupiv_fn fn, void *ud);

/* Look up the aggregate for a (row_key, col_key) pair. Returns 0 if found and
 * sets *v; returns -1 if either key is absent. */
int wubupiv_get(const wubupiv *p, const char *rowkey, const char *colkey, double *v);

void wubupiv_free(wubupiv *p);

#endif
