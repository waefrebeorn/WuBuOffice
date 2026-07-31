/* history.c -- opaque version-history store. See history.h. */
#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIST_MAX 512

typedef struct {
    int   id;
    char *blob;
    size_t len;
    char *label;
    char *author;
} Ver;

struct History {
    Ver v[HIST_MAX];
    int n;
    int next_id;
};

History *history_create(void){
    History *h = calloc(1, sizeof *h);
    if (h) h->next_id = 1;
    return h;
}

static void ver_free(Ver *v){
    free(v->blob); free(v->label); free(v->author);
    memset(v, 0, sizeof *v);
}

void history_destroy(History *h){
    if (!h) return;
    for (int i=0;i<h->n;i++) ver_free(&h->v[i]);
    free(h);
}

static Ver *find(History *h, int id){
    for (int i=0;i<h->n;i++) if (h->v[i].id == id) return &h->v[i];
    return NULL;
}

int history_commit(History *h, const char *blob, size_t len,
                   const char *label, const char *author){
    if (!h || !blob || h->n >= HIST_MAX) return 0;
    Ver *v = &h->v[h->n++];
    v->id = h->next_id++;
    v->blob = malloc(len ? len : 1);
    if (!v->blob){ h->n--; return 0; }
    memcpy(v->blob, blob, len); v->len = len;
    v->label = label ? strdup(label) : NULL;
    v->author = author ? strdup(author) : NULL;
    return v->id;
}

int history_count(const History *h){ return h ? h->n : 0; }
int history_id_at(const History *h, int i){
    if (!h || i < 0 || i >= h->n) return 0;
    return h->v[i].id;
}
const char *history_blob(const History *h, int id, size_t *out_len){
    if (out_len) *out_len = 0;
    Ver *v = find((History*)h, id);
    if (!v) return NULL;
    if (out_len) *out_len = v->len;
    return v->blob;
}
const char *history_author(const History *h, int id){ Ver *v = find((History*)h, id); return v? v->author : NULL; }
const char *history_label(const History *h, int id){ Ver *v = find((History*)h, id); return v? v->label : NULL; }

/* simple LCS line diff */
char *history_diff(History *h, int a, int b){
    if (!h) return NULL;
    Ver *va = find(h, a), *vb = find(h, b);
    if (!va || !vb) return NULL;
    /* split blobs into lines */
    #define MAXL 1024
    char *la[MAXL], *lb[MAXL]; int na=0, nb=0;
    char *ca = malloc(va->len+1), *cb = malloc(vb->len+1);
    if (!ca || !cb){ free(ca); free(cb); return NULL; }
    memcpy(ca, va->blob, va->len); ca[va->len]=0;
    memcpy(cb, vb->blob, vb->len); cb[vb->len]=0;
    char *p = ca;
    while (p && *p && na < MAXL){ char *nl = strchr(p,'\n'); if (nl) *nl=0; la[na++] = p; p = nl? nl+1 : NULL; }
    p = cb;
    while (p && *p && nb < MAXL){ char *nl = strchr(p,'\n'); if (nl) *nl=0; lb[nb++] = p; p = nl? nl+1 : NULL; }
    /* LCS table */
    int L[MAXL+1][MAXL+1];
    for (int i=0;i<=na;i++) for (int j=0;j<=nb;j++) L[i][j]=0;
    for (int i=na-1;i>=0;i--)
        for (int j=nb-1;j>=0;j--)
            L[i][j] = !strcmp(la[i],lb[j]) ? L[i+1][j+1]+1 : (L[i+1][j]>L[i][j+1]? L[i+1][j]:L[i][j+1]);
    size_t cap=256, len=0; char *out = malloc(cap);
    if (!out){ free(ca); free(cb); return NULL; }
    int i=0,j=0;
    while (i<na && j<nb){
        if (!strcmp(la[i],lb[j])){ i++; j++; }
        else if (L[i+1][j] >= L[i][j+1]){
            len += (size_t)snprintf(out+len, cap-len, "-%s\n", la[i]); i++;
        } else {
            len += (size_t)snprintf(out+len, cap-len, "+%s\n", lb[j]); j++;
        }
        while (len+64 > cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);free(ca);free(cb);return NULL;} out=no; }
    }
    while (i<na){ len += (size_t)snprintf(out+len, cap-len, "-%s\n", la[i++]); }
    while (j<nb){ len += (size_t)snprintf(out+len, cap-len, "+%s\n", lb[j++]); }
    free(ca); free(cb);
    if (len == 0){ free(out); return NULL; }  /* equal */
    out[len] = 0;
    return out;
    #undef MAXL
}
