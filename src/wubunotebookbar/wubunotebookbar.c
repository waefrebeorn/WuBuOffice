#include "wubunotebookbar.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; } tab;

struct wubunotebookbar { tab *tabs; size_t n, cap, active; };

wubunotebookbar *wubunotebookbar_create(void){ return (wubunotebookbar *)calloc(1,sizeof(wubunotebookbar)); }
void wubunotebookbar_destroy(wubunotebookbar *n){
    if (!n) return;
    for (size_t i=0;i<n->n;i++) free(n->tabs[i].name);
    free(n->tabs); free(n);
}
int wubunotebookbar_add(wubunotebookbar *n, const char *name){
    if (!n || !name) return -1;
    if (n->n == n->cap){ size_t nc = n->cap?n->cap*2:8; tab *nt=(tab*)realloc(n->tabs,nc*sizeof(tab)); if(!nt) return -1; n->tabs=nt; n->cap=nc; }
    n->tabs[n->n].name = strdup(name);
    if (!n->tabs[n->n].name) return -1;
    n->n++;
    return 0;
}
size_t wubunotebookbar_count(const wubunotebookbar *n){ return n?n->n:0; }
const char *wubunotebookbar_name(const wubunotebookbar *n, size_t i){ return (n&&i<n->n)?n->tabs[i].name:NULL; }
int wubunotebookbar_set_active(wubunotebookbar *n, size_t i){ if(!n||i>=n->n) return -1; n->active=i; return 0; }
size_t wubunotebookbar_active(const wubunotebookbar *n){ return n?n->active:0; }
