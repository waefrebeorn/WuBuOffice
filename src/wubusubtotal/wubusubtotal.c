#include "wubusubtotal.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

double wubusub_value(const wubusub_group *g, wubusub_fn fn) {
    switch (fn) {
        case WUBUSUB_SUM:    return g->sum;
        case WUBUSUB_AVG:    return g->n ? g->sum / (double)g->n : 0;
        case WUBUSUB_COUNT:  return (double)g->n;
        case WUBUSUB_MIN:    return g->n ? g->mn : 0;
        case WUBUSUB_MAX:    return g->n ? g->mx : 0;
    }
    return 0;
}

int wubusub_aggregate(void **rows, size_t n,
                      wubusub_keyfn key, wubusub_valfn val, wubusub_fn fn, void *ud,
                      wubusub_group **out, size_t *nout) {
    if (!rows || !key || !val || !out || !nout) return -1;
    *out = NULL; *nout = 0;
    if (n == 0) return 0;

    size_t cap = 4, cnt = 0;
    wubusub_group *g = (wubusub_group *)malloc(cap * sizeof(wubusub_group));
    if (!g) return -1;

    for (size_t i = 0; i < n; i++) {
        const char *k = key(rows[i], ud);
        const char *v = val(rows[i], ud);
        if (!k) k = "";
        /* find existing group */
        size_t gi = (size_t)-1;
        for (size_t j = 0; j < cnt; j++)
            if (strcmp(g[j].group, k) == 0) { gi = j; break; }
        if (gi == (size_t)-1) {
            if (cnt == cap) {
                cap *= 2;
                wubusub_group *ng = (wubusub_group *)realloc(g, cap * sizeof(wubusub_group));
                if (!ng) { wubusub_free(g, cnt); return -1; }
                g = ng;
            }
            gi = cnt++;
            g[gi].group = strdup(k);
            g[gi].sum = 0; g[gi].n = 0; g[gi].mn = 0; g[gi].mx = 0;
        }
        wubusub_group *gg = &g[gi];
        if (v && *v) {
            errno = 0; double x = strtod(v, NULL);
            if (!errno) {
                if (gg->n == 0) { gg->mn = gg->mx = x; }
                else { if (x < gg->mn) gg->mn = x; if (x > gg->mx) gg->mx = x; }
                gg->sum += x;
                gg->n++;
            }
        }
    }
    *out = g; *nout = cnt;
    return 0;
}

void wubusub_free(wubusub_group *g, size_t n) {
    if (!g) return;
    for (size_t i = 0; i < n; i++) free(g[i].group);
    free(g);
}
