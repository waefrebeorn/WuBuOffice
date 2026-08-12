#include "wubuhyperlink.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint64_t id; wubuhyperlink_entry e; } link_pair;

struct wubuhyperlink {
    link_pair *items;
    size_t n, cap;
};

wubuhyperlink *wubuhyperlink_create(void) {
    return (wubuhyperlink *)calloc(1, sizeof(wubuhyperlink));
}

static void free_entry(wubuhyperlink_entry *e) {
    free(e->target); free(e->text); free(e->anchor);
    e->target = e->text = e->anchor = NULL;
}

void wubuhyperlink_destroy(wubuhyperlink *h) {
    if (!h) return;
    for (size_t i = 0; i < h->n; i++) free_entry(&h->items[i].e);
    free(h->items);
    free(h);
}

static int find(const wubuhyperlink *h, uint64_t id) {
    for (size_t i = 0; i < h->n; i++) if (h->items[i].id == id) return (int)i;
    return -1;
}

int wubuhyperlink_set(wubuhyperlink *h, uint64_t node_id,
                      const char *target, const char *text, const char *anchor) {
    if (!h || !target) return -1;
    int i = find(h, node_id);
    if (i < 0) {
        if (h->n == h->cap) {
            size_t nc = h->cap ? h->cap * 2 : 4;
            link_pair *ni = (link_pair *)realloc(h->items, nc * sizeof(link_pair));
            if (!ni) return -1;
            h->items = ni; h->cap = nc;
        }
        i = (int)h->n++;
        h->items[i].id = node_id;
        h->items[i].e.target = NULL; h->items[i].e.text = NULL; h->items[i].e.anchor = NULL;
    }
    wubuhyperlink_entry *e = &h->items[i].e;
    char *nt = strdup(target), *ntx = text ? strdup(text) : NULL, *na = anchor ? strdup(anchor) : NULL;
    if (!nt || (text && !ntx) || (anchor && !na)) { free(nt); free(ntx); free(na); return -1; }
    free_entry(e);
    e->target = nt; e->text = ntx; e->anchor = na;
    return 0;
}

const wubuhyperlink_entry *wubuhyperlink_get(const wubuhyperlink *h, uint64_t node_id) {
    if (!h) return NULL;
    int i = find(h, node_id);
    return i < 0 ? NULL : &h->items[i].e;
}

int wubuhyperlink_remove(wubuhyperlink *h, uint64_t node_id) {
    if (!h) return 0;
    int i = find(h, node_id);
    if (i < 0) return 0;
    free_entry(&h->items[i].e);
    memmove(&h->items[i], &h->items[i + 1], (h->n - (size_t)i - 1) * sizeof(link_pair));
    h->n--;
    return 1;
}

size_t wubuhyperlink_count(const wubuhyperlink *h) { return h ? h->n : 0; }
