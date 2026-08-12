#include "wubufilter.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

double wubufilter_num(const char *s) {
    if (!s) return 0;
    errno = 0; double x = strtod(s, NULL);
    return errno ? 0 : x;
}

static int empty(const char *s) { return !s || *s == '\0'; }

static int one(void *row, const wubufilter_crit *c, wubufilter_cellfn cell, void *ud) {
    const char *v = cell(row, c->col, ud);
    switch (c->op) {
        case WUBUFILTER_BLANK:    return empty(v);
        case WUBUFILTER_NOTBLANK: return !empty(v);
        default: break;
    }
    if (!c->value) return 0;
    switch (c->op) {
        case WUBUFILTER_EQ: return !empty(v) && strcmp(v, c->value) == 0;
        case WUBUFILTER_NEQ: return empty(v) || strcmp(v, c->value) != 0;
        case WUBUFILTER_CONTAINS: {
            if (empty(v)) return 0;
            /* case-insensitive substring */
            const char *a = v, *b = c->value;
            size_t lb = strlen(b); if (!lb) return 1;
            for (; *a; a++) {
                size_t i; int m = 1;
                for (i = 0; i < lb; i++)
                    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) { m = 0; break; }
                if (m) return 1;
            }
            return 0;
        }
        case WUBUFILTER_GT:  return wubufilter_num(v) >  wubufilter_num(c->value);
        case WUBUFILTER_GTE: return wubufilter_num(v) >= wubufilter_num(c->value);
        case WUBUFILTER_LT:  return wubufilter_num(v) <  wubufilter_num(c->value);
        case WUBUFILTER_LTE: return wubufilter_num(v) <= wubufilter_num(c->value);
        case WUBUFILTER_BETWEEN: {
            double x = wubufilter_num(v);
            char buf[128]; strncpy(buf, c->value, sizeof buf - 1); buf[sizeof buf - 1] = 0;
            char *sep = strchr(buf, '|');
            if (!sep) return 0;
            *sep = 0;
            double lo = wubufilter_num(buf), hi = wubufilter_num(sep + 1);
            return x >= lo && x <= hi;
        }
        case WUBUFILTER_TOP: {
            int n = atoi(c->value); if (n < 1) n = 1;
            double x = wubufilter_num(v);
            /* deferred: computed in apply pass; placeholder here */
            return x == x && x >= -1e300; /* noop, handled in pass */
        }
        default: return 0;
    }
}

int wubufilter_apply(void **rows, size_t nrows,
                     const wubufilter_crit *crit, int ncrit,
                     wubufilter_cellfn cell, void *ud,
                     size_t *out, size_t *n) {
    if (!rows || !crit || ncrit < 1 || !cell || !out || !n) return -1;
    if (!nrows) { *n = 0; return 0; }

    /* TOP is a whole-column operation: precompute the top-N indices. */
    size_t *topmask = NULL;
    for (int k = 0; k < ncrit; k++) {
        if (crit[k].op == WUBUFILTER_TOP) {
            topmask = (size_t *)calloc(nrows, sizeof(size_t));
            if (!topmask) return -1;
            int N = atoi(crit[k].value); if (N < 1) N = 1;
            if ((size_t)N > nrows) N = (int)nrows;
            /* selection of largest N by value at column */
            for (int t = 0; t < N; t++) {
                size_t best = (size_t)-1; double bestv = -1e300;
                for (size_t i = 0; i < nrows; i++) {
                    if (topmask[i]) continue;
                    double x = wubufilter_num(cell(rows[i], crit[k].col, ud));
                    if (best == (size_t)-1 || x > bestv) { best = i; bestv = x; }
                }
                if (best != (size_t)-1) topmask[best] = 1;
            }
        }
    }

    size_t c = 0;
    for (size_t i = 0; i < nrows; i++) {
        int ok = 1;
        for (int k = 0; k < ncrit; k++) {
            if (crit[k].op == WUBUFILTER_TOP) { if (!topmask[i]) { ok = 0; break; } continue; }
            if (!one(rows[i], &crit[k], cell, ud)) { ok = 0; break; }
        }
        if (ok) out[c++] = i;
    }
    if (topmask) free(topmask);
    *n = c;
    return 0;
}
