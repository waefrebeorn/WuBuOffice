/* col.c -- opaque comment-thread store. See col.h. */
#include "col.h"

#include <stdlib.h>
#include <string.h>

#define COL_MAX_THREADS 512
#define COL_MAX_REPLIES 64

typedef struct {
    int   id;
    int   resolved;
    char *anchor;
    char *author;
    char *text;
    Reply replies[COL_MAX_REPLIES];
    int   nrep;
} Thread;

struct Col {
    Thread t[COL_MAX_THREADS];
    int n;
    int next_id;
};

Col *col_create(void){ Col *c = calloc(1, sizeof *c); if (c) c->next_id=1; return c; }

static void thread_free(Thread *t){
    free(t->anchor); free(t->author); free(t->text);
    for (int i=0;i<t->nrep;i++){ free(t->replies[i].author); free(t->replies[i].text); }
    memset(t, 0, sizeof *t);
}

void col_destroy(Col *c){
    if (!c) return;
    for (int i=0;i<c->n;i++) thread_free(&c->t[i]);
    free(c);
}

static Thread *find(Col *c, int tid){
    for (int i=0;i<c->n;i++) if (c->t[i].id==tid) return &c->t[i];
    return NULL;
}

int col_add(Col *c, const char *anchor, const char *author, const char *text){
    if (!c || !anchor || c->n>=COL_MAX_THREADS) return 0;
    Thread *t = &c->t[c->n++];
    t->id = c->next_id++;
    t->anchor = strdup(anchor);
    t->author = author ? strdup(author) : NULL;
    t->text   = text ? strdup(text) : strdup("");
    t->resolved = 0;
    return t->id;
}

int col_reply(Col *c, int tid, const char *author, const char *text){
    Thread *t = find(c, tid);
    if (!t || t->nrep>=COL_MAX_REPLIES) return 0;
    Reply *r = &t->replies[t->nrep++];
    r->author = author ? strdup(author) : NULL;
    r->text   = text ? strdup(text) : strdup("");
    return 1;
}

int col_resolve(Col *c, int tid, int resolved){
    Thread *t = find(c, tid);
    if (!t) return 0;
    t->resolved = resolved ? 1 : 0;
    return 1;
}

int col_remove(Col *c, int tid){
    Thread *t = find(c, tid);
    if (!t) return 0;
    thread_free(t);
    /* compact */
    for (int i=(int)(t - c->t); i<c->n-1; i++) c->t[i] = c->t[i+1];
    c->n--;
    return 1;
}

int col_thread_count(const Col *c){ return c? c->n : 0; }
int col_id_at(const Col *c, int i){ if (!c||i<0||i>=c->n) return 0; return c->t[i].id; }
const char *col_anchor(const Col *c, int tid){ Thread *t=find((Col*)c,tid); return t? t->anchor : NULL; }
const char *col_author(const Col *c, int tid){ Thread *t=find((Col*)c,tid); return t? t->author : NULL; }
const char *col_text(const Col *c, int tid){ Thread *t=find((Col*)c,tid); return t? t->text : NULL; }
int col_resolved(const Col *c, int tid){ Thread *t=find((Col*)c,tid); return t? t->resolved : 0; }
int col_reply_count(const Col *c, int tid){ Thread *t=find((Col*)c,tid); return t? t->nrep : 0; }
const Reply *col_reply_at(const Col *c, int tid, int i){
    Thread *t=find((Col*)c,tid);
    if (!t || i<0 || i>=t->nrep) return NULL;
    return &t->replies[i];
}
