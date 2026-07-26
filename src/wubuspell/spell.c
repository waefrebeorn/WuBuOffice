/* spell.c -- dependency-free C11 spell checker (see spell.h). */
#include "spell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* --- word storage: open-addressing hash set of normalized words --- */
typedef struct {
    char  *word;   /* normalized (ASCII-lowercased) copy, NUL-terminated */
    int    order;  /* insertion order (tie-break for suggestion ranking) */
} Slot;

struct SpellDict {
    Slot  *slots;
    int    cap;      /* power of two */
    int    count;    /* live entries */
    int    seq;      /* insertion counter */
    /* ignore list (small, linear) */
    char **ignore;
    int    ign_n, ign_cap;
};

/* FNV-1a over bytes. */
static uint64_t fnv1a(const char *s, size_t n) {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 1099511628211ULL; }
    return h;
}

/* Normalize: ASCII-lowercase into a heap copy. Non-ASCII bytes kept as-is. */
static char *normalize(const char *word) {
    size_t n = strlen(word);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)word[i];
        out[i] = (c < 0x80) ? (char)tolower(c) : (char)c;
    }
    out[n] = 0;
    return out;
}

static int is_all_digits(const char *w) {
    if (!*w) return 0;
    for (const char *p = w; *p; p++)
        if (!isdigit((unsigned char)*p) && *p != '.' && *p != ',') return 0;
    return 1;
}

/* find slot index for normalized key; returns index (empty or match). */
static int find_slot(const SpellDict *d, const char *key, size_t klen, int *found) {
    uint64_t h = fnv1a(key, klen);
    int mask = d->cap - 1;
    int i = (int)(h & (uint64_t)mask);
    for (;;) {
        if (!d->slots[i].word) { *found = 0; return i; }
        if (strcmp(d->slots[i].word, key) == 0) { *found = 1; return i; }
        i = (i + 1) & mask;
    }
}

static int rehash(SpellDict *d, int newcap) {
    Slot *ns = calloc((size_t)newcap, sizeof *ns);
    if (!ns) return -1;
    Slot *old = d->slots; int oldcap = d->cap;
    d->slots = ns; d->cap = newcap;
    for (int i = 0; i < oldcap; i++) {
        if (!old[i].word) continue;
        int found = 0;
        int idx = find_slot(d, old[i].word, strlen(old[i].word), &found);
        d->slots[idx] = old[i];
    }
    free(old);
    return 0;
}

SpellDict *spell_create(void) {
    SpellDict *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->cap = 1024;
    d->slots = calloc((size_t)d->cap, sizeof *d->slots);
    if (!d->slots) { free(d); return NULL; }
    return d;
}

void spell_free(SpellDict *d) {
    if (!d) return;
    for (int i = 0; i < d->cap; i++) free(d->slots[i].word);
    free(d->slots);
    for (int i = 0; i < d->ign_n; i++) free(d->ignore[i]);
    free(d->ignore);
    free(d);
}

int spell_add_word(SpellDict *d, const char *word) {
    if (!d || !word || !*word) return 0;
    char *key = normalize(word);
    if (!key) return 0;
    if ((d->count + 1) * 4 >= d->cap * 3) {   /* keep load factor < 0.75 */
        if (rehash(d, d->cap * 2) != 0) { free(key); return 0; }
    }
    int found = 0;
    int idx = find_slot(d, key, strlen(key), &found);
    if (found) { free(key); return 0; }
    d->slots[idx].word = key;
    d->slots[idx].order = d->seq++;
    d->count++;
    return 1;
}

int spell_load(SpellDict *d, const char *path) {
    if (!d || !path) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char line[512];
    int added = 0;
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0) continue;
        /* cut at whitespace / tab / newline (keeps "word" from "word\tcount") */
        char *q = p;
        while (*q && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') q++;
        *q = 0;
        if (*p) added += spell_add_word(d, p);
    }
    fclose(f);
    return added;
}

int spell_size(const SpellDict *d) { return d ? d->count : 0; }

static int in_ignore(const SpellDict *d, const char *key) {
    for (int i = 0; i < d->ign_n; i++)
        if (strcmp(d->ignore[i], key) == 0) return 1;
    return 0;
}

void spell_ignore(SpellDict *d, const char *word) {
    if (!d || !word || !*word) return;
    char *key = normalize(word);
    if (!key) return;
    if (in_ignore(d, key)) { free(key); return; }
    if (d->ign_n >= d->ign_cap) {
        int nc = d->ign_cap ? d->ign_cap * 2 : 16;
        char **ni = realloc(d->ignore, (size_t)nc * sizeof *ni);
        if (!ni) { free(key); return; }
        d->ignore = ni; d->ign_cap = nc;
    }
    d->ignore[d->ign_n++] = key;
}

int spell_check(SpellDict *d, const char *word) {
    if (!d || !word || !*word) return 1;      /* nothing to flag */
    if (is_all_digits(word)) return 1;
    char *key = normalize(word);
    if (!key) return 1;
    int ok = 0;
    if (in_ignore(d, key)) ok = 1;
    else { int found = 0; find_slot(d, key, strlen(key), &found); ok = found; }
    free(key);
    return ok;
}

/* --- suggestions: Levenshtein over bytes, ranked by (distance, order) --- */
static int levenshtein(const char *a, const char *b, int cutoff) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (abs(la - lb) > cutoff) return cutoff + 1;
    int *prev = malloc((size_t)(lb + 1) * sizeof *prev);
    int *cur  = malloc((size_t)(lb + 1) * sizeof *cur);
    if (!prev || !cur) { free(prev); free(cur); return cutoff + 1; }
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        cur[0] = i;
        int rowmin = cur[0];
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = cur[j-1] + 1;
            int sub = prev[j-1] + cost;
            int v = del < ins ? del : ins;
            if (sub < v) v = sub;
            cur[j] = v;
            if (v < rowmin) rowmin = v;
        }
        if (rowmin > cutoff) { free(prev); free(cur); return cutoff + 1; }
        int *t = prev; prev = cur; cur = t;
    }
    int r = prev[lb];
    free(prev); free(cur);
    return r;
}

typedef struct { const char *word; int dist; int order; } Cand;

static int cand_cmp(const void *pa, const void *pb) {
    const Cand *a = pa, *b = pb;
    if (a->dist != b->dist) return a->dist - b->dist;
    return a->order - b->order;
}

int spell_suggest(SpellDict *d, const char *word, char **out, int max) {
    if (!d || !word || !*word || max <= 0) return 0;
    char *key = normalize(word);
    if (!key) return 0;
    int klen = (int)strlen(key);
    int cutoff = (klen <= 4) ? 1 : 2;   /* stricter for short words */

    Cand *cands = malloc((size_t)d->count * sizeof *cands);
    if (!cands) { free(key); return 0; }
    int nc = 0;
    for (int i = 0; i < d->cap; i++) {
        if (!d->slots[i].word) continue;
        int dl = abs((int)strlen(d->slots[i].word) - klen);
        if (dl > cutoff) continue;
        int dist = levenshtein(key, d->slots[i].word, cutoff);
        if (dist <= cutoff && dist > 0) {
            cands[nc].word = d->slots[i].word;
            cands[nc].dist = dist;
            cands[nc].order = d->slots[i].order;
            nc++;
        }
    }
    qsort(cands, (size_t)nc, sizeof *cands, cand_cmp);
    int w = 0;
    for (int i = 0; i < nc && w < max; i++) {
        char *s = strdup(cands[i].word);
        if (!s) break;
        out[w++] = s;
    }
    free(cands);
    free(key);
    return w;
}

/* --- text scanning: tokenize UTF-8 into words --- */
/* A "word" byte: ASCII letter, apostrophe (internal), or any non-ASCII byte
 * (so accented / CJK words are captured as tokens). Digits break words unless
 * the whole token is a number (handled by spell_check). */
static int is_word_byte(unsigned char c) {
    if (c >= 0x80) return 1;                 /* UTF-8 continuation / lead */
    if (isalpha(c)) return 1;
    return 0;
}

int spell_scan(SpellDict *d, const char *utf8, SpellError *out, int max) {
    if (!d || !utf8) return 0;
    int n = (int)strlen(utf8);
    int errors = 0;
    int i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)utf8[i];
        if (!is_word_byte(c)) { i++; continue; }
        int start = i;
        /* consume the word: word bytes plus internal straight apostrophes */
        while (i < n) {
            unsigned char b = (unsigned char)utf8[i];
            if (is_word_byte(b)) { i++; continue; }
            if (b == '\'' && i + 1 < n && is_word_byte((unsigned char)utf8[i+1])) {
                i++; continue;   /* internal apostrophe: don't / it's */
            }
            break;
        }
        int len = i - start;
        if (len <= 0) { i++; continue; }
        /* extract token */
        char tok[256];
        int tl = len < 255 ? len : 255;
        memcpy(tok, utf8 + start, (size_t)tl);
        tok[tl] = 0;
        if (!spell_check(d, tok)) {
            if (errors < max && out) { out[errors].offset = start; out[errors].len = len; }
            errors++;
        }
    }
    return errors;
}
