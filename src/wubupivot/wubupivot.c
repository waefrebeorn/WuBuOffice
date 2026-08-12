#include "wubupivot.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int idx_of(char **keys, size_t n, const char *k) {
    for (size_t i = 0; i < n; i++) if (strcmp(keys[i], k) == 0) return (int)i;
    return -1;
}

static int ensure_key(char ***keys, size_t *n, size_t *cap, const char *k) {
    if (idx_of(*keys, *n, k) >= 0) return idx_of(*keys, *n, k);
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 4;
        char **nk = (char **)realloc(*keys, nc * sizeof(char *));
        if (!nk) return -1;
        *keys = nk; *cap = nc;
    }
    (*keys)[*n] = strdup(k);
    return (int)(*n)++;
}

static void acc(double *slot, size_t *cnt, double x, wubupiv_fn fn) {
    switch (fn) {
        case WUBUPIV_SUM:   *slot += x; break;
        case WUBUPIV_COUNT: *slot += 1; break;
        case WUBUPIV_AVG:   *slot += x; (*cnt)++; break;
        case WUBUPIV_MIN:   if (*cnt == 0) *slot = x; else if (x < *slot) *slot = x; (*cnt)++; break;
        case WUBUPIV_MAX:   if (*cnt == 0) *slot = x; else if (x > *slot) *slot = x; (*cnt)++; break;
    }
}

wubupiv *wubupiv_build(void **rows, size_t n,
                       wubupiv_rowfn rfn, wubupiv_colfn cfn, wubupiv_valfn vfn,
                       wubupiv_fn fn, void *ud) {
    if (!rows || !rfn || !cfn || !vfn) return NULL;
    wubupiv *p = (wubupiv *)calloc(1, sizeof(wubupiv));
    if (!p) return NULL;
    p->fn = fn;
    p->total_rows = n;

    size_t rcap = 0, ccap = 0;
    size_t *cellcnt = NULL;

    /* first pass: discover keys */
    for (size_t i = 0; i < n; i++) {
        const char *rk = rfn(rows[i], ud), *ck = cfn(rows[i], ud);
        if (!rk) rk = "";
        if (!ck) ck = "";
        if (ensure_key(&p->row_keys, &p->nrows, &rcap, rk) < 0) goto oom;
        if (ensure_key(&p->col_keys, &p->ncols, &ccap, ck) < 0) goto oom;
    }
    p->cells = (double *)calloc(p->nrows * p->ncols, sizeof(double));
    cellcnt = (size_t *)calloc(p->nrows * p->ncols, sizeof(size_t));
    if (!p->cells || !cellcnt) goto oom;

    /* second pass: accumulate */
    for (size_t i = 0; i < n; i++) {
        const char *rk = rfn(rows[i], ud), *ck = cfn(rows[i], ud);
        if (!rk) rk = "";
        if (!ck) ck = "";
        int ri = idx_of(p->row_keys, p->nrows, rk);
        int ci = idx_of(p->col_keys, p->ncols, ck);
        if (ri < 0 || ci < 0) continue;
        const char *v = vfn(rows[i], ud);
        double x = 0; int has = 0;
        if (v && *v) { errno = 0; x = strtod(v, NULL); if (!errno) has = 1; }
        if (fn == WUBUPIV_COUNT) has = 1; /* count counts every row */
        if (has) acc(&p->cells[(size_t)ri * p->ncols + ci], &cellcnt[(size_t)ri * p->ncols + ci], x, fn);
    }
    /* finalize AVG */
    if (fn == WUBUPIV_AVG)
        for (size_t i = 0; i < p->nrows * p->ncols; i++)
            if (cellcnt[i]) p->cells[i] /= (double)cellcnt[i];

    free(cellcnt);
    return p;
oom:
    free(cellcnt);
    wubupiv_free(p);
    return NULL;
}

int wubupiv_get(const wubupiv *p, const char *rowkey, const char *colkey, double *v) {
    if (!p) return -1;
    int ri = idx_of(p->row_keys, p->nrows, rowkey);
    int ci = idx_of(p->col_keys, p->ncols, colkey);
    if (ri < 0 || ci < 0) return -1;
    if (v) *v = p->cells[(size_t)ri * p->ncols + ci];
    return 0;
}

void wubupiv_free(wubupiv *p) {
    if (!p) return;
    for (size_t i = 0; i < p->nrows; i++) free(p->row_keys[i]);
    for (size_t i = 0; i < p->ncols; i++) free(p->col_keys[i]);
    free(p->row_keys); free(p->col_keys); free(p->cells);
    free(p);
}
