#include "wubuicon.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; char *data; } icon;

struct wubuicon { icon *items; size_t n, cap; };

wubuicon *wubuicon_create(void) { return (wubuicon *)calloc(1, sizeof(wubuicon)); }

void wubuicon_destroy(wubuicon *i) {
    if (!i) return;
    for (size_t k = 0; k < i->n; k++) { free(i->items[k].name); free(i->items[k].data); }
    free(i->items);
    free(i);
}

static int find(const wubuicon *i, const char *name) {
    for (size_t k = 0; k < i->n; k++) if (strcmp(i->items[k].name, name) == 0) return (int)k;
    return -1;
}

int wubuicon_add(wubuicon *i, const char *name, const char *data) {
    if (!i || !name || !data) return -1;
    int idx = find(i, name);
    if (idx < 0) {
        if (i->n == i->cap) {
            size_t nc = i->cap ? i->cap * 2 : 8;
            icon *ni = (icon *)realloc(i->items, nc * sizeof(icon));
            if (!ni) return -1;
            i->items = ni; i->cap = nc;
        }
        idx = (int)i->n++;
        i->items[idx].name = strdup(name);
        i->items[idx].data = NULL;
    }
    free(i->items[idx].data);
    i->items[idx].data = strdup(data);
    return (i->items[idx].data) ? 0 : -1;
}

const char *wubuicon_get(const wubuicon *i, const char *name) {
    if (!i) return NULL;
    int idx = find(i, name);
    return idx < 0 ? NULL : i->items[idx].data;
}

size_t wubuicon_count(const wubuicon *i) { return i ? i->n : 0; }
const char *wubuicon_name(const wubuicon *i, size_t idx) {
    return (i && idx < i->n) ? i->items[idx].name : NULL;
}
