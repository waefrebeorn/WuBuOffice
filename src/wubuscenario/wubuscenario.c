#include "wubuscenario.h"
#include <stdlib.h>
#include <string.h>

struct wubuscenario {
    wubuscen_entry *entries;
    size_t n, cap;
};

wubuscenario *wubuscenario_create(void) {
    return (wubuscenario *)calloc(1, sizeof(wubuscenario));
}

void wubuscenario_destroy(wubuscenario *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) {
        free(s->entries[i].name);
        for (size_t j = 0; j < s->entries[i].n; j++) free(s->entries[i].cells[j].value);
        free(s->entries[i].cells);
    }
    free(s->entries);
    free(s);
}

int wubuscenario_set(wubuscenario *s, const char *name,
                     const wubuscen_cell *cells, size_t n) {
    if (!s || !name) return -1;
    /* find existing */
    size_t i;
    for (i = 0; i < s->n; i++) if (strcmp(s->entries[i].name, name) == 0) break;
    if (i == s->n) { /* new */
        if (s->n == s->cap) {
            size_t nc = s->cap ? s->cap * 2 : 4;
            wubuscen_entry *ne = (wubuscen_entry *)realloc(s->entries, nc * sizeof(wubuscen_entry));
            if (!ne) return -1;
            s->entries = ne; s->cap = nc;
        }
        s->n++;
        s->entries[i].name = strdup(name);
        s->entries[i].cells = NULL; s->entries[i].n = 0;
    }
    /* free old cells */
    for (size_t j = 0; j < s->entries[i].n; j++) free(s->entries[i].cells[j].value);
    free(s->entries[i].cells);
    s->entries[i].cells = NULL; s->entries[i].n = 0;
    if (n > 0) {
        s->entries[i].cells = (wubuscen_cell *)calloc(n, sizeof(wubuscen_cell));
        if (!s->entries[i].cells) return -1;
        for (size_t j = 0; j < n; j++) {
            s->entries[i].cells[j].row = cells[j].row;
            s->entries[i].cells[j].col = cells[j].col;
            s->entries[i].cells[j].value = strdup(cells[j].value ? cells[j].value : "");
        }
        s->entries[i].n = n;
    }
    return 0;
}

const wubuscen_entry *wubuscenario_get(const wubuscenario *s, const char *name) {
    if (!s || !name) return NULL;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->entries[i].name, name) == 0) return &s->entries[i];
    return NULL;
}

size_t wubuscenario_count(const wubuscenario *s) { return s ? s->n : 0; }
const char *wubuscenario_name(const wubuscenario *s, size_t i) {
    return (s && i < s->n) ? s->entries[i].name : NULL;
}

int wubuscenario_apply(const wubuscenario *s, const char *name,
                       int (*apply)(int row, int col, const char *value, void *ud),
                       void *ud) {
    const wubuscen_entry *e = wubuscenario_get(s, name);
    if (!e || !apply) return -1;
    for (size_t i = 0; i < e->n; i++)
        apply(e->cells[i].row, e->cells[i].col, e->cells[i].value, ud);
    return 0;
}
