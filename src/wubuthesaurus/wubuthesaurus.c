#include "wubuthesaurus.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { char *word; char **syns; size_t n; } entry;

struct wubuthesaurus {
    entry *entries;
    size_t n, cap;
};

static char *dup_lower(const char *s) {
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

wubuthesaurus *wubuthesaurus_create(void) {
    return (wubuthesaurus *)calloc(1, sizeof(wubuthesaurus));
}

void wubuthesaurus_destroy(wubuthesaurus *t) {
    if (!t) return;
    for (size_t i = 0; i < t->n; i++) {
        free(t->entries[i].word);
        for (size_t j = 0; j < t->entries[i].n; j++) free(t->entries[i].syns[j]);
        free(t->entries[i].syns);
    }
    free(t->entries);
    free(t);
}

int wubuthesaurus_add(wubuthesaurus *t, const char *word, const char **words) {
    if (!t || !word || !words) return -1;
    char *w = dup_lower(word);
    if (!w) return -1;
    size_t n = 0; while (words[n]) n++;
    /* find existing */
    size_t i;
    for (i = 0; i < t->n; i++) if (strcmp(t->entries[i].word, w) == 0) break;
    int is_new = (i == t->n);
    if (is_new) {
        if (t->n == t->cap) {
            size_t nc = t->cap ? t->cap * 2 : 8;
            entry *ne = (entry *)realloc(t->entries, nc * sizeof(entry));
            if (!ne) { free(w); return -1; }
            t->entries = ne; t->cap = nc;
        }
        i = t->n++;
        t->entries[i].word = w;
        t->entries[i].syns = NULL; t->entries[i].n = 0;
    } else {
        free(w);
    }
    /* merge (replace) syns */
    for (size_t j = 0; j < t->entries[i].n; j++) free(t->entries[i].syns[j]);
    free(t->entries[i].syns);
    char **syns = n ? (char **)calloc(n + 1, sizeof(char *)) : NULL;
    if (n && !syns) return -1;
    for (size_t j = 0; j < n; j++) {
        syns[j] = strdup(words[j]);
        if (!syns[j]) { for (size_t k = 0; k < j; k++) free(syns[k]); free(syns); return -1; }
    }
    t->entries[i].syns = syns; t->entries[i].n = n;
    return 0;
}

const char **wubuthesaurus_lookup(const wubuthesaurus *t, const char *word) {
    if (!t || !word) return NULL;
    char *w = dup_lower(word);
    if (!w) return NULL;
    const char **out = NULL;
    for (size_t i = 0; i < t->n; i++)
        if (strcmp(t->entries[i].word, w) == 0) { out = (const char **)t->entries[i].syns; break; }
    free(w);
    return out;
}

size_t wubuthesaurus_count(const wubuthesaurus *t) { return t ? t->n : 0; }
