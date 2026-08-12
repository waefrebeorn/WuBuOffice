#include "wubugallery.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; wubugallery_col c; } gitem;

struct wubugallery { gitem *items; size_t n, cap; };

wubugallery *wubugallery_create(void) { return (wubugallery *)calloc(1, sizeof(wubugallery)); }

void wubugallery_destroy(wubugallery *g) {
    if (!g) return;
    for (size_t k = 0; k < g->n; k++) {
        free(g->items[k].name);
        for (size_t j = 0; j < g->items[k].c.n; j++) free(g->items[k].c.items[j]);
        free(g->items[k].c.items);
    }
    free(g->items);
    free(g);
}

static wubugallery_col *find_col(wubugallery *g, const char *name) {
    for (size_t k = 0; k < g->n; k++) if (strcmp(g->items[k].name, name) == 0) return &g->items[k].c;
    return NULL;
}

static int ensure_gallery(wubugallery *g, const char *name) {
    if (find_col(g, name)) return 0;
    if (g->n == g->cap) {
        size_t nc = g->cap ? g->cap * 2 : 4;
        gitem *ni = (gitem *)realloc(g->items, nc * sizeof(gitem));
        if (!ni) return -1;
        g->items = ni; g->cap = nc;
    }
    g->items[g->n].name = strdup(name);
    g->items[g->n].c.items = NULL;
    g->items[g->n].c.n = 0;
    g->n++;
    return 0;
}

wubugallery_col *wubugallery_get(wubugallery *g, const char *name) {
    if (!g || !name) return NULL;
    if (ensure_gallery(g, name) != 0) return NULL;
    return find_col(g, name);
}

int wubugallery_add_item(wubugallery *g, const char *name, const char *item) {
    if (!g || !name || !item) return -1;
    if (ensure_gallery(g, name) != 0) return -1;
    wubugallery_col *c = find_col(g, name);
    char **ni = (char **)realloc(c->items, (c->n + 1) * sizeof(char *));
    if (!ni) return -1;
    c->items = ni;
    c->items[c->n] = strdup(item);
    if (!c->items[c->n]) return -1;
    c->n++;
    return 0;
}

size_t wubugallery_count(wubugallery *g) { return g ? g->n : 0; }
const char *wubugallery_name(wubugallery *g, size_t i) {
    return (g && i < g->n) ? g->items[i].name : NULL;
}
