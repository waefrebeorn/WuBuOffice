/* crdt.c -- node-sequence CRDT (op-based, LWW-merge). See crdt.h. */
#include "crdt.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CRDT_MAX_ITEMS 4096
#define CRDT_ID_LEN    18

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

/* deterministic order: by (lamport, id) */
static int item_order(const Item *a, const Item *b){
    if (a->lamport != b->lamport) return a->lamport < b->lamport ? -1 : 1;
    return strcmp(a->id, b->id);
}

/* insert `it` into the sorted array at the correct (lamport,id) position */
static void sorted_insert(Crdt *c, Item it){
    int lo = 0, hi = c->n;
    while (lo < hi){
        int mid = (lo+hi)/2;
        if (item_order(&c->items[mid], &it) < 0) lo = mid+1; else hi = mid;
    }
    memmove(&c->items[lo+1], &c->items[lo], (size_t)(c->n-lo)*sizeof(Item));
    c->items[lo] = it;
    c->n++;
}

Crdt *crdt_create(const char *site){
    Crdt *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    if (site) strncpy(c->site, site, CRDT_ID_LEN-1);
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

static int find_pos(const Crdt *c, int pos){
    /* pos is a logical (live) index; walk live items in clock order */
    int live = 0;
    for (int i=0;i<c->n;i++){
        if (!c->items[i].tomb){
            if (live == pos) return i;
            live++;
        }
    }
    return c->n; /* append */
}

static const char *make_id(Crdt *c, char *out){
    snprintf(out, CRDT_ID_LEN, "%u:%s", ++c->clock, c->site);
    return out;
}

const char *crdt_insert(Crdt *c, int pos, const char *value){
    if (!c || c->n >= CRDT_MAX_ITEMS) return NULL;
    char id[CRDT_ID_LEN]; make_id(c, id);
    int at = find_pos(c, pos < 0 ? 0 : pos);
    Item it; memset(&it, 0, sizeof it);
    strncpy(it.id, id, CRDT_ID_LEN-1);
    it.lamport = c->clock;
    it.tomb = 0;
    it.value = value ? strdup(value) : strdup("");
    /* shift to keep sorted order by (lamport,id) */
    memmove(&c->items[at+1], &c->items[at], (size_t)(c->n-at)*sizeof(Item));
    c->items[at] = it;
    c->n++;
    return it.id;
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
    sorted_insert(c, it);
    return 1;
}

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

int crdt_merge(Crdt *dst, const Crdt *src){
    if (!dst || !src) return 0;
    int added = 0;
    for (int i=0;i<src->n;i++){
        const Item *s = &src->items[i];
        int found = 0;
        for (int j=0;j<dst->n;j++)
            if (!strcmp(dst->items[j].id, s->id)){ found = 1; break; }
        if (found) continue;
        if (dst->n >= CRDT_MAX_ITEMS) break;
        Item it; memset(&it, 0, sizeof it);
        strncpy(it.id, s->id, CRDT_ID_LEN-1);
        it.lamport = s->lamport;
        it.tomb = s->tomb;
        it.value = s->value ? strdup(s->value) : strdup("");
        sorted_insert(dst, it);
        added++;
        if (s->lamport > dst->clock) dst->clock = s->lamport;
    }
    return added;
}

char *crdt_serialize(const Crdt *c, size_t *out_len){
    if (!c || !out_len) return NULL;
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    for (int i=0;i<c->n;i++){
        const Item *it = &c->items[i];
        int need = snprintf(NULL,0,"%s %u %d %s\n", it->id, it->lamport, it->tomb, it->value? it->value:"");
        while (len + (size_t)need + 1 > cap){ cap *= 2; char *nb = realloc(buf, cap); if(!nb){ free(buf); return NULL;} buf = nb; }
        len += (size_t)sprintf(buf+len, "%s %u %d %s\n", it->id, it->lamport, it->tomb, it->value? it->value:"");
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
        char line[1024]; if (llen >= sizeof line) llen = sizeof line -1;
        memcpy(line, p, llen); line[llen]=0;
        char id[CRDT_ID_LEN]; unsigned lamp; int tomb; char val[512];
        if (sscanf(line, "%17s %u %d %511[^\n]", id, &lamp, &tomb, val) == 4){
            if (c->n < CRDT_MAX_ITEMS){
                Item *it = &c->items[c->n++];
                memset(it, 0, sizeof *it);
                strncpy(it->id, id, CRDT_ID_LEN-1);
                it->lamport = lamp; it->tomb = tomb;
                it->value = strdup(val);
                if (lamp > c->clock) c->clock = lamp;
            }
        }
        p = nl ? nl+1 : end;
    }
    return 1;
}
