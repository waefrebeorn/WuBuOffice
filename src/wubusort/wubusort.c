#include "wubusort.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static int numcmp_raw(const char *a, const char *b) {
    double x = 0, y = 0;
    if (a) { errno = 0; x = strtod(a, NULL); if (errno) x = 0; }
    if (b) { errno = 0; y = strtod(b, NULL); if (errno) y = 0; }
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

int wubusort_numcmp(const char *a, const char *b) {
    return numcmp_raw(a, b);
}

static int lc_lt(const char *a, const char *b) {
    if (!a) a = ""; if (!b) b = "";
    while (*a && *b) {
        unsigned char ca = (unsigned char)tolower((unsigned char)*a);
        unsigned char cb = (unsigned char)tolower((unsigned char)*b);
        if (ca != cb) return ca < cb;
        a++; b++;
    }
    return *a == 0 && *b != 0;
}

typedef struct {
    void **rows; size_t n; void **tmp;
    const wubusort_col *cols; int ncols;
    wubusort_cellfn cell; void *ud;
} sort_ctx;

static int cmp_at(const sort_ctx *ctx, void *ra, void *rb) {
    int k;
    for (k = 0; k < ctx->ncols; k++) {
        const wubusort_col *c = &ctx->cols[k];
        const char *ca = ctx->cell(ra, c->col, ctx->ud);
        const char *cb = ctx->cell(rb, c->col, ctx->ud);
        int d = c->numeric ? numcmp_raw(ca, cb) : (lc_lt(ca, cb) ? -1 : (lc_lt(cb, ca) ? 1 : 0));
        if (d != 0) return c->desc ? -d : d;
    }
    return 0;
}

/* stable bottom-up merge sort */
static void merge_runs(sort_ctx *ctx, size_t lo, size_t mid, size_t hi) {
    size_t i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (cmp_at(ctx, ctx->rows[i], ctx->rows[j]) <= 0)
            ctx->tmp[k++] = ctx->rows[i++];
        else
            ctx->tmp[k++] = ctx->rows[j++];
    }
    while (i < mid) ctx->tmp[k++] = ctx->rows[i++];
    while (j < hi)  ctx->tmp[k++] = ctx->rows[j++];
    for (i = lo; i < hi; i++) ctx->rows[i] = ctx->tmp[i];
}

int wubusort_rows(void **rows, size_t n, const wubusort_col *cols, int ncols,
                  wubusort_cellfn cell, void *ud) {
    if (!rows || n == 0) return 0;
    if (!cols || ncols < 1 || !cell) return -1;
    sort_ctx ctx;
    ctx.rows = rows; ctx.n = n; ctx.cols = cols; ctx.ncols = ncols;
    ctx.cell = cell; ctx.ud = ud;
    ctx.tmp = (void **)malloc(n * sizeof(void *));
    if (!ctx.tmp) return -1;
    size_t width = 1;
    while (width < n) {
        size_t lo = 0;
        while (lo < n) {
            size_t mid = lo + width; if (mid > n) mid = n;
            size_t hi  = lo + 2 * width; if (hi > n) hi = n;
            if (mid < hi) merge_runs(&ctx, lo, mid, hi);
            lo = hi;
        }
        width *= 2;
    }
    free(ctx.tmp);
    return 0;
}
