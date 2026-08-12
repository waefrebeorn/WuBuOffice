#include "wubuindex.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *dup_lower(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

wubuindex *wubuindex_create(void) {
    return (wubuindex *)calloc(1, sizeof(wubuindex));
}

void wubuindex_destroy(wubuindex *ix) {
    if (!ix) return;
    for (size_t i = 0; i < ix->n; i++) { free(ix->entries[i].term); free(ix->entries[i].pages); }
    free(ix->entries);
    free(ix);
}

static int find(const wubuindex *ix, const char *term) {
    for (size_t i = 0; i < ix->n; i++)
        if (strcmp(ix->entries[i].term, term) == 0) return (int)i;
    return -1;
}

int wubuindex_add_term(wubuindex *ix, const char *term) {
    if (!ix || !term) return -1;
    char *w = dup_lower(term);
    if (!w) return -1;
    if (find(ix, w) >= 0) { free(w); return 0; }
    if (ix->n == ix->cap) {
        size_t nc = ix->cap ? ix->cap * 2 : 8;
        wubuindex_entry *ne = (wubuindex_entry *)realloc(ix->entries, nc * sizeof(wubuindex_entry));
        if (!ne) { free(w); return -1; }
        ix->entries = ne; ix->cap = nc;
    }
    /* insert keeping entries sorted by term */
    size_t pos = ix->n;
    for (size_t i = 0; i < ix->n; i++)
        if (strcmp(w, ix->entries[i].term) < 0) { pos = i; break; }
    memmove(&ix->entries[pos + 1], &ix->entries[pos], (ix->n - pos) * sizeof(wubuindex_entry));
    ix->entries[pos].term = w;
    ix->entries[pos].pages = NULL;
    ix->entries[pos].npages = 0;
    ix->entries[pos].cap = 0;
    ix->n++;
    return 0;
}

int wubuindex_feed_page(wubuindex *ix, const char *page_text, int page) {
    if (!ix || !page_text || page < 0) return -1;
    const char *t = page_text;
    size_t len = strlen(t);
    for (size_t i = 0; i < ix->n; i++) {
        const char *term = ix->entries[i].term;
        size_t tlen = strlen(term);
        if (!tlen) continue;
        int found = 0;
        for (size_t pos = 0; pos + tlen <= len; pos++) {
            size_t k; int m = 1;
            for (k = 0; k < tlen; k++)
                if (tolower((unsigned char)t[pos+k]) != (unsigned char)term[k]) { m=0; break; }
            if (m) { found = 1; break; }
        }
        if (found) {
            wubuindex_entry *e = &ix->entries[i];
            /* insert page sorted-ascending, dedupe */
            size_t j;
            for (j = 0; j < e->npages; j++) if (e->pages[j] == page) break;
            if (j < e->npages) continue; /* dup */
            for (j = 0; j < e->npages; j++) if (page < e->pages[j]) break;
            if (e->npages == e->cap) {
                size_t nc = e->cap ? e->cap * 2 : 4;
                int *np = (int *)realloc(e->pages, nc * sizeof(int));
                if (!np) return -1;
                e->pages = np; e->cap = nc;
            }
            memmove(&e->pages[j+1], &e->pages[j], (e->npages - j) * sizeof(int));
            e->pages[j] = page;
            e->npages++;
        }
    }
    return 0;
}

size_t wubuindex_count(const wubuindex *ix) { return ix ? ix->n : 0; }
const wubuindex_entry *wubuindex_get(const wubuindex *ix, size_t i) {
    return (ix && i < ix->n) ? &ix->entries[i] : NULL;
}
