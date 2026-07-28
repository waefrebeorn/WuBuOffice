/* cite.c -- bibliography / citation store. See cite.h. */
#include "cite.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CITE_MAX 512
typedef struct {
    char key[64]; char type[32]; char title[256]; char authors[256]; int year;
} Entry;

struct Cite { Entry e[CITE_MAX]; int n; };

Cite *cite_create(void){ return calloc(1, sizeof(Cite)); }
void cite_destroy(Cite *c){ free(c); }

int cite_add(Cite *c, const char *key, const char *type, const char *title, const char *authors, int year){
    if (!c || !key || c->n>=CITE_MAX) return 0;
    strncpy(c->e[c->n].key, key, 63); c->e[c->n].key[63]=0;
    strncpy(c->e[c->n].type, type?type:"", 31); c->e[c->n].type[31]=0;
    strncpy(c->e[c->n].title, title?title:"", 255); c->e[c->n].title[255]=0;
    strncpy(c->e[c->n].authors, authors?authors:"", 255); c->e[c->n].authors[255]=0;
    c->e[c->n].year = year;
    c->n++;
    return 1;
}

static Entry *find(Cite *c, const char *key){
    for (int i=0;i<c->n;i++) if (!strcmp(c->e[i].key, key)) return &c->e[i];
    return NULL;
}

char *cite_inline(Cite *c, const char *key){
    if (!c || !key) return NULL;
    Entry *e = find(c, key);
    if (!e) return NULL;
    /* surname = last whitespace-delimited token */
    char sur[64]; sur[0]=0;
    const char *tok = e->authors;
    for (const char *p = e->authors; ; p++){
        if (*p==' ' || *p=='\0'){
            size_t l = (size_t)(p - tok);
            if (l>0 && l < sizeof sur){ memcpy(sur, tok, l); sur[l]=0; }
            tok = p+1;
        }
        if (*p=='\0') break;
    }
    char *out = malloc(64);
    if (!out) return NULL;
    sprintf(out, "(%s, %d)", sur[0]?sur:"?", e->year);
    return out;
}

char *cite_bibliography(Cite *c){
    if (!c) return NULL;
    size_t cap=256, len=0; char *out=malloc(cap);
    if (!out) return NULL;
    out[0]=0;
    for (int i=0;i<c->n;i++){
        char line[1024];
        snprintf(line,sizeof line,"[%d] %s. %s. %s.\n", i+1,
                 c->e[i].authors, c->e[i].title, c->e[i].type);
        size_t need=strlen(line);
        while (len+need+1>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
        memcpy(out+len, line, need); len+=need;
    }
    out[len]=0;
    return out;
}

int cite_count(const Cite *c){ return c? c->n : 0; }
