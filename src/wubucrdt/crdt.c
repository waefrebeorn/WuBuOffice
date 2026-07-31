/* crdt.c -- node-sequence CRDT (op-based, LWW-merge). See crdt.h.
 *
 * Sequence of Items kept sorted by (lamport, id). Each Item carries:
 *   - id       : unique "clock:site" tag used for ordering and dedup
 *   - lamport  : logical clock; bumped on every local op + on every merge
 *   - tomb     : 1 = deleted (item still occupies its slot until compaction)
 *   - value    : malloc'd payload (NULL becomes "")
 *
 * Merge semantics: union of items, LWW per id. Items are kept in sorted
 * order so duplicates collide and are skipped. -Wreturn-local-addr warnings
 * are avoided by copying the full Item struct into the heap slot before any
 * function returns a pointer into it.
 *
 * Clean C11, self-contained (only libc). */
#include "crdt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRDT_MAX_ITEMS 4096
#define CRDT_ID_LEN    18   /* "<clock>:" + short site; 17 chars + NUL */

typedef struct {
    char  id[CRDT_ID_LEN];   /* "<clock>:" + site; unique */
    unsigned lamport;
    int   tomb;
    char *value;
} Item;

struct Crdt {
    char  site[CRDT_ID_LEN];
    unsigned clock;
    Item  items[CRDT_MAX_ITEMS];
    int   n;
};

/* ---- internal helpers ---- */

/* deterministic order: by (lamport, id) */
static int item_order(const Item *a, const Item *b){
    if (a->lamport != b->lamport) return a->lamport < b->lamport ? -1 : 1;
    return strcmp(a->id, b->id);
}

/* Copy `src` into the sorted array at the correct (lamport,id) position.
 * The src Item is copied by-value; the caller still owns any malloc'd fields
 * in `src`. Returns the slot index the item ended up in (or -1 on overflow). */
static int sorted_insert(Crdt *c, const Item *src){
    int lo = 0, hi = c->n;
    while (lo < hi){
        int mid = (lo+hi)/2;
        if (item_order(&c->items[mid], src) < 0) lo = mid+1; else hi = mid;
    }
    if (c->n >= CRDT_MAX_ITEMS) return -1;
    memmove(&c->items[lo+1], &c->items[lo], (size_t)(c->n-lo)*sizeof(Item));
    c->items[lo] = *src;
    c->n++;
    return lo;
}

/* Safe bounded string copy with explicit NUL termination. Avoids the
 * -Wstringop-truncation false positive when the source is exactly the
 * destination size. `dst_cap` must be >= 1. */
static void safe_copy(char *dst, size_t dst_cap, const char *src){
    if (dst_cap == 0) return;
    size_t n = src ? strlen(src) : 0;
    if (n >= dst_cap) n = dst_cap - 1;
    memcpy(dst, src ? src : "", n);
    dst[n] = '\0';
}

/* ---- lifecycle ---- */

Crdt *crdt_create(const char *site){
    Crdt *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    safe_copy(c->site, sizeof c->site, site);
    return c;
}

static void free_items(Crdt *c){
    for (int i=0;i<c->n;i++) free(c->items[i].value);
    c->n = 0;
}

void crdt_destroy(Crdt *c){
    if (!c) return;
    free_items(c);
    free(c);
}

/* ---- mutation ---- */

/* `pos` is a logical (live) index; walk live items in clock order. */
static int find_pos(const Crdt *c, int pos){
    int live = 0;
    for (int i=0;i<c->n;i++){
        if (!c->items[i].tomb){
            if (live == pos) return i;
            live++;
        }
    }
    return c->n; /* append */
}

static void make_id(Crdt *c, char out[CRDT_ID_LEN]){
    /* "<clock>:<site>" — CRDT_ID_LEN=18 is tight; cap the %s width so the
     * longest possible id ("4294967295:site1234") always fits with NUL. */
    snprintf(out, CRDT_ID_LEN, "%u:%.8s", ++c->clock, c->site);
}

const char *crdt_insert(Crdt *c, int pos, const char *value){
    if (!c || c->n >= CRDT_MAX_ITEMS) return NULL;
    int at = find_pos(c, pos < 0 ? 0 : pos);
    Item it;
    memset(&it, 0, sizeof it);
    make_id(c, it.id);
    it.lamport = c->clock;
    it.tomb = 0;
    it.value = strdup(value ? value : "");
    if (!it.value){ it.tomb = 1; return NULL; }
    /* shift to keep sorted order by (lamport,id), then copy in. */
    memmove(&c->items[at+1], &c->items[at], (size_t)(c->n-at)*sizeof(Item));
    c->items[at] = it;
    c->n++;
    /* Return a pointer into the heap-resident slot, not the local. */
    return c->items[at].id;
}

int crdt_delete(Crdt *c, int pos){
    if (!c) return 0;
    int at = find_pos(c, pos);
    if (at >= c->n || c->items[at].tomb) return 0;
    c->items[at].tomb = 1;
    return 1;
}

int crdt_move(Crdt *c, int from, int to){
    if (!c) return 0;
    int fi = find_pos(c, from);
    if (fi >= c->n || c->items[fi].tomb) return 0;
    Item it = c->items[fi];
    /* bump clock so the moved item sorts after concurrent ops */
    it.lamport = ++c->clock;
    /* remove from current slot */
    memmove(&c->items[fi], &c->items[fi+1], (size_t)(c->n-fi-1)*sizeof(Item));
    c->n--;
    /* re-insert in sorted position */
    sorted_insert(c, &it);
    return 1;
}

/* ---- read ---- */

int crdt_count(const Crdt *c){
    if (!c) return 0;
    int live = 0;
    for (int i=0;i<c->n;i++) if (!c->items[i].tomb) live++;
    return live;
}

const char *crdt_get(const Crdt *c, int pos){
    if (!c || pos < 0) return NULL;
    int live = 0;
    for (int i=0;i<c->n;i++)
        if (!c->items[i].tomb){
            if (live == pos) return c->items[i].value;
            live++;
        }
    return NULL;
}

/* ---- merge ---- */

int crdt_merge(Crdt *dst, const Crdt *src){
    if (!dst || !src) return 0;
    int added = 0;
    for (int i=0;i<src->n;i++){
        const Item *s = &src->items[i];
        int found = 0;
        for (int j=0;j<dst->n;j++)
            if (!strcmp(dst->items[j].id, s->id)){ found = 1; break; }
        if (found) continue;
        Item it;
        memset(&it, 0, sizeof it);
        safe_copy(it.id, sizeof it.id, s->id);
        it.lamport = s->lamport;
        it.tomb = s->tomb;
        it.value = strdup(s->value ? s->value : "");
        if (!it.value) continue;
        if (sorted_insert(dst, &it) < 0){ free(it.value); break; }
        added++;
        if (s->lamport > dst->clock) dst->clock = s->lamport;
    }
    return added;
}

/* ---- serialize ---- */

char *crdt_serialize(const Crdt *c, size_t *out_len){
    if (!c || !out_len) return NULL;
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (int i=0;i<c->n;i++){
        const Item *it = &c->items[i];
        const char *val = it->value ? it->value : "";
        int need = snprintf(NULL, 0, "%s %u %d %s\n",
                            it->id, it->lamport, it->tomb, val);
        while (len + (size_t)need + 1 > cap){
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb){ free(buf); return NULL; }
            buf = nb;
        }
        len += (size_t)snprintf(buf+len, cap-len, "%s %u %d %s\n",
                                it->id, it->lamport, it->tomb, val);
    }
    *out_len = len;
    return buf;
}

int crdt_deserialize(Crdt *c, const char *buf, size_t len){
    if (!c || !buf) return 0;
    free_items(c);
    const char *p = buf;
    const char *end = buf + len;
    while (p < end){
        const char *nl = memchr(p, '\n', (size_t)(end-p));
        size_t llen = nl ? (size_t)(nl-p) : (size_t)(end-p);
        char line[1024];
        if (llen >= sizeof line) llen = sizeof line - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';
        char id[CRDT_ID_LEN];
        unsigned lamp;
        int tomb;
        char val[512];
        /* %17s caps id at 17 chars (+NUL by snprintf); %511[^\\n] caps value. */
        if (sscanf(line, "%17s %u %d %511[^\n]",
                   id, &lamp, &tomb, val) == 4){
            if (c->n < CRDT_MAX_ITEMS){
                Item *it = &c->items[c->n++];
                memset(it, 0, sizeof *it);
                safe_copy(it->id, sizeof it->id, id);
                it->lamport = lamp;
                it->tomb = tomb;
                it->value = strdup(val);
                if (lamp > c->clock) c->clock = lamp;
            }
        }
        p = nl ? nl + 1 : end;
    }
    return 1;
}
