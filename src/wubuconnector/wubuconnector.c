#include "wubuconnector.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char from[64], fromport[16], to[64], toport[16];
} conn;

struct wubuconnector { conn *items; size_t n, cap; };

wubuconnector *wubuconnector_create(void) { return (wubuconnector *)calloc(1, sizeof(wubuconnector)); }

void wubuconnector_destroy(wubuconnector *c) {
    if (!c) return;
    free(c->items);
    free(c);
}

int wubuconnector_add(wubuconnector *c, const char *from, const char *fromport,
                      const char *to, const char *toport) {
    if (!c || !from || !to) return -1;
    if (c->n == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 4;
        conn *ni = (conn *)realloc(c->items, nc * sizeof(conn));
        if (!ni) return -1;
        c->items = ni; c->cap = nc;
    }
    conn *e = &c->items[c->n];
    strncpy(e->from, from, sizeof e->from - 1); e->from[sizeof e->from - 1] = 0;
    strncpy(e->to, to, sizeof e->to - 1); e->to[sizeof e->to - 1] = 0;
    strncpy(e->fromport, fromport ? fromport : "", sizeof e->fromport - 1); e->fromport[sizeof e->fromport - 1] = 0;
    strncpy(e->toport, toport ? toport : "", sizeof e->toport - 1); e->toport[sizeof e->toport - 1] = 0;
    c->n++;
    return 0;
}

size_t wubuconnector_count(const wubuconnector *c) { return c ? c->n : 0; }
const char *wubuconnector_from(const wubuconnector *c, size_t i) { return (c && i < c->n) ? c->items[i].from : NULL; }
const char *wubuconnector_to(const wubuconnector *c, size_t i) { return (c && i < c->n) ? c->items[i].to : NULL; }

int wubuconnector_route(const wubuconnector *c, size_t i,
                        const wubuc_rect *a, const wubuc_rect *b,
                        float p[6]) {
    if (!c || i >= c->n || !a || !b || !p) return -1;
    /* exit source right-center */
    float sx = a->x + a->w;
    float sy = a->y + a->h * 0.5f;
    /* enter target left-center */
    float tx = b->x;
    float ty = b->y + b->h * 0.5f;
    /* clean orthogonal L: horizontal run to the target's x, then vertical drop
       to the target's mid-height. elbow = (tx, sy). */
    p[0] = sx; p[1] = sy;     /* start (source right edge) */
    p[2] = tx; p[3] = sy;     /* elbow (horizontal run) */
    p[4] = tx; p[5] = ty;     /* end (vertical drop to target) */
    return 0;
}
