#include "wubusmartart.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *text; } node;

struct wubusmartart {
    wubusa_layout layout;
    node *nodes;
    size_t n, cap;
};

wubusmartart *wubusmartart_create(void){
    wubusmartart *s = (wubusmartart*)calloc(1,sizeof(wubusmartart));
    if (s) s->layout = WUBU_SA_PROCESS;
    return s;
}
void wubusmartart_destroy(wubusmartart *s){
    if (!s) return;
    for (size_t i=0;i<s->n;i++) free(s->nodes[i].text);
    free(s->nodes); free(s);
}
int wubusmartart_set_layout(wubusmartart *s, wubusa_layout layout){
    if (!s || layout < WUBU_SA_PROCESS || layout > WUBU_SA_LIST) return -1;
    s->layout = layout; return 0;
}
wubusa_layout wubusmartart_layout(const wubusmartart *s){ return s?s->layout:0; }
int wubusmartart_add_node(wubusmartart *s, const char *text){
    if (!s || !text) return -1;
    if (s->n == s->cap){ size_t nc=s->cap?s->cap*2:8; node*nn=(node*)realloc(s->nodes,nc*sizeof(node)); if(!nn) return -1; s->nodes=nn; s->cap=nc; }
    s->nodes[s->n].text = strdup(text);
    if (!s->nodes[s->n].text) return -1;
    s->n++;
    return 0;
}
size_t wubusmartart_count(const wubusmartart *s){ return s?s->n:0; }
const char *wubusmartart_node(const wubusmartart *s, size_t i){ return (s&&i<s->n)?s->nodes[i].text:NULL; }
