/* wubusubtotal.h — spreadsheet SUBTOTAL: group rows by a key column and
 * compute an aggregate (sum/avg/count/min/max) over a value column. */
#ifndef WUBUSUBTOTAL_H
#define WUBUSUBTOTAL_H
#include <stddef.h>

typedef enum {
    WUBUSUB_SUM, WUBUSUB_AVG, WUBUSUB_COUNT, WUBUSUB_MIN, WUBUSUB_MAX
} wubusub_fn;

typedef struct {
    char *group;          /* owned copy of the group key string */
    double sum; size_t n; /* n = number of non-NULL numeric cells */
    double mn, mx;
} wubusub_group;

typedef const char *(*wubusub_keyfn)(void *row, void *ud);  /* group key cell */
typedef const char *(*wubusub_valfn)(void *row, void *ud);  /* value cell */

/* Group `rows[0..n)` by key, aggregate value cells per group, and write the
 * resulting groups (in first-seen key order) to `*out`. Caller frees each
 * returned group with `wubusub_free(out, *nout)`. Returns 0 on success. */
int wubusub_aggregate(void **rows, size_t n,
                      wubusub_keyfn key, wubusub_valfn val, wubusub_fn fn, void *ud,
                      wubusub_group **out, size_t *nout);

/* Compute the aggregate value of one group under `fn`. */
double wubusub_value(const wubusub_group *g, wubusub_fn fn);

void wubusub_free(wubusub_group *g, size_t n);

#endif
